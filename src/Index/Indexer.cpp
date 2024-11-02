#include <clang/AST/DeclCXX.h>
#include <clang/Index/USRGeneration.h>

#include <Compiler/Compiler.h>
#include <Index/Indexer.h>
#include <Support/FileSystem.h>
#include <Support/Reflection.h>

namespace clice::index {

namespace {

static clang::Decl* declForType(clang::QualType type) {
    if(type.isNull()) {
        return nullptr;
    }

    if(auto RT = llvm::dyn_cast<clang::TagType>(type)) {
        return RT->getDecl();
    } else if(auto TT = llvm::dyn_cast<clang::TypedefType>(type)) {
        return TT->getDecl();
    }

    return nullptr;
}

/// The `Indexer` is used to collect data from the AST and generate an index.
class IndexBuilder {
public:
    IndexBuilder(clang::Sema& sema, const clang::syntax::TokenBuffer& tokBuf) :
        sema(sema), context(sema.getASTContext()), srcMgr(context.getSourceManager()),
        tokBuf(tokBuf) {}

    uint32_t addFile(clang::FileID id) {
        auto& files = result.files;
        auto index = files.size();
        auto [iter, success] = fileCache.try_emplace(id, index);
        auto result = iter->second;

        if(success) {
            files.emplace_back();
            if(auto entry = srcMgr.getFileEntryRefForID(id)) {
                llvm::SmallString<128> path;
                if(auto error = fs::real_path(entry->getName(), path)) {
                    llvm::outs() << error.message() << "\n";
                }
                // FIXME: relative path.
                files.back().path = path.str();
                auto include = toLocation(srcMgr.getIncludeLoc(id));
                files[index].include = include;
            } else {
                std::terminate();
            }
        }

        return result;
    }

    Location toLocation(clang::SourceRange range) {
        Location location = {};

        if(range.isInvalid() || range.getBegin().isMacroID() || range.getEnd().isMacroID()) {
            return location;
        }

        auto beginLoc = srcMgr.getPresumedLoc(range.getBegin());
        auto endLoc = srcMgr.getPresumedLoc(
            clang::Lexer::getLocForEndOfToken(range.getEnd(), 0, srcMgr, sema.getLangOpts()));

        /// FIXME: position encoding?
        location.begin.line = beginLoc.getLine();
        location.begin.column = beginLoc.getColumn();
        location.end.line = endLoc.getLine();
        location.end.column = endLoc.getColumn();
        location.file = addFile(beginLoc.getFileID());

        return location;
    }

    friend class SymbolRef;

    struct SymbolRef {
        IndexBuilder& builder;
        uint32_t index;

        const memory::SymbolID& id() const {
            return builder.result.symbols[index].id;
        }

        SymbolRef addRelation(RelationKind kind,
                              const Location& location,
                              const memory::SymbolID& related = {}) {
            auto& relations = builder.result.symbols[index].relations;
            relations.emplace_back(static_cast<RelationKind>(kind), location, related);
            return *this;
        }

        SymbolRef addRelation(uint32_t kind, const Location& location) {
            auto& relations = builder.result.symbols[index].relations;
            relations.emplace_back(static_cast<RelationKind>(kind), location);
            return *this;
        }

        SymbolRef addReference(const Location& location) {
            return addRelation(RelationKind::Reference, location);
        }

        // A definition is also a declaration and a reference.
        SymbolRef addDefinition(const Location& location) {
            return addRelation(RelationKind(RelationKind::Reference,
                                            RelationKind::Declaration,
                                            RelationKind::Definition),
                               location);
        }

        SymbolRef addDeclarationOrDefinition(bool is_definition, const Location& location) {
            RelationKind kind = RelationKind::Declaration;
            if(is_definition) {
                kind.set(RelationKind::Definition);
            }
            return addRelation(kind, location);
        }

        SymbolRef addTypeDefinition(clang::QualType type) {
            if(auto decl = declForType(type)) {
                return addRelation(RelationKind::TypeDefinition,
                                   builder.toLocation(decl->getLocation()));
            }
            return *this;
        }

        SymbolRef addOccurrence(const Location& location) {
            builder.result.occurrences.emplace_back(location, index);
            return *this;
        }

        /// Should be called by caller, add `Caller` and `Callee` relation between two symbols.
        SymbolRef addCall(const Location& location, const clang::Decl* callee) {
            auto symbol = builder.addSymbol(llvm::dyn_cast<clang::NamedDecl>(callee));
            symbol.addRelation(RelationKind::Caller, location, id());
            return addRelation(RelationKind::Callee, location, symbol.id());
        }
    };

    static memory::SymbolID generateSymbolID(const clang::Decl* decl) {
        llvm::SmallString<128> USR;
        clang::index::generateUSRForDecl(decl, USR);
        return llvm::hash_value(USR);
    }

    /// Add a symbol to the index.
    SymbolRef addSymbol(const clang::NamedDecl* decl) {
        if(decl == nullptr) {
            std::terminate();
        }

        auto& symbols = result.symbols;
        auto canonical = decl->getCanonicalDecl();

        auto [iter, success] = symbolCache.try_emplace(canonical, symbols.size());
        uint32_t index = iter->second;

        if(success) {
            memory::Symbol& symbol = symbols.emplace_back();
            symbol.id = generateSymbolID(canonical);
            symbol.name = decl->getNameAsString();
        }

        return SymbolRef{*this, index};
    }

    memory::Index index() &&;

private:
    clang::Sema& sema;
    clang::ASTContext& context;
    clang::SourceManager& srcMgr;
    const clang::syntax::TokenBuffer& tokBuf;
    /// The result index.
    memory::Index result;
    /// A map between canonical decl and calculated data.
    llvm::DenseMap<clang::FileID, std::size_t> fileCache;
    llvm::DenseMap<const clang::Decl*, std::size_t> symbolCache;
};

class IndexCollector : public clang::RecursiveASTVisitor<IndexCollector> {
    using Base = clang::RecursiveASTVisitor<IndexCollector>;

public:
    IndexCollector(IndexBuilder& indexer, clang::ASTContext& context) :
        indexer(indexer), context(context), srcMgr(context.getSourceManager()) {}

    /// we don't care about the node without location information, so skip them.
    constexpr bool shouldWalkTypesOfTypeLocs [[gnu::const]] () {
        return false;
    }

    /// ============================================================================
    ///                                Declaration
    /// ============================================================================

    bool VisitNamespaceDecl(const clang::NamespaceDecl* decl) {
        /// `namespace Foo {}`
        ///             ^~~~ definition
        if(Location location = indexer.toLocation(decl->getLocation())) {
            auto symbol = indexer.addSymbol(decl);
            symbol.addOccurrence(location);
            symbol.addDefinition(location);
        }
        return true;
    }

    bool VisitNamespaceAliasDecl(const clang::NamespaceAliasDecl* decl) {
        /// `namespace Foo = Bar`
        ///             ^     ^~~~ reference
        ///             ^~~~ definition
        if(Location location = indexer.toLocation(decl->getLocation())) {
            auto symbol = indexer.addSymbol(decl);
            symbol.addOccurrence(location);
            symbol.addDefinition(location);
        }
        if(Location location = indexer.toLocation(decl->getTargetNameLoc())) {
            auto symbol = indexer.addSymbol(decl->getNamespace());
            symbol.addOccurrence(location);
            symbol.addReference(location);
        }
        return true;
    }

    bool VisitUsingDirectiveDecl(const clang::UsingDirectiveDecl* decl) {
        /// `using namespace Foo`
        ///                  ^^^~~~~~~ reference
        if(Location location = indexer.toLocation(decl->getLocation())) {
            auto symbol = indexer.addSymbol(decl->getNominatedNamespace());
            symbol.addOccurrence(location);
            symbol.addReference(location);
        }
        return true;
    }

    bool VisitLabelDecl(const clang::LabelDecl* decl) {
        /// `label:`
        ///    ^~~~ definition
        if(Location location = indexer.toLocation(decl->getLocation())) {
            auto symbol = indexer.addSymbol(decl);
            symbol.addOccurrence(location);
            symbol.addDefinition(location);
        }
        return true;
    }

    bool VisitTagDecl(const clang::TagDecl* decl) {
        if(Location location = indexer.toLocation(decl->getLocation())) {
            auto symbol = indexer.addSymbol(decl);
            symbol.addOccurrence(location);
            symbol.addDefinition(location);
        }
        return true;
    }

    bool VisitFieldDecl(const clang::FieldDecl* decl) {
        if(Location location = indexer.toLocation(decl->getLocation())) {
            auto symbol = indexer.addSymbol(decl);
            symbol.addOccurrence(location);
            symbol.addDefinition(location);
            symbol.addTypeDefinition(decl->getType());
        }
        return true;
    }

    bool VisitEnumConstantDecl(const clang::EnumConstantDecl* decl) {
        if(Location location = indexer.toLocation(decl->getLocation())) {
            auto symbol = indexer.addSymbol(decl);
            symbol.addOccurrence(location);
            symbol.addDefinition(location);
            symbol.addTypeDefinition(decl->getType());
        }
        return true;
    }

    /// `TypedefDecl` and `TypedefNameDecl` are both inherited from `TypedefNameDecl`.
    /// So we only need to handle `TypedefNameDecl`.
    /// FIXME: `TypeAliasTemplateDecl`.
    bool VisitTypedefNameDecl(const clang::TypedefNameDecl* TND) {
        if(Location location = indexer.toLocation(TND->getLocation())) {
            auto symbol = indexer.addSymbol(TND);
            symbol.addOccurrence(location);
            symbol.addDefinition(location);
            symbol.addTypeDefinition(TND->getUnderlyingType());
        }
        return true;
    }

    bool VisitVarDecl(const clang::VarDecl* decl) {
        if(Location location = indexer.toLocation(decl->getLocation())) {
            auto symbol = indexer.addSymbol(decl);
            symbol.addOccurrence(location);
            symbol.addDeclarationOrDefinition(decl->isThisDeclarationADefinition(), location);
            symbol.addTypeDefinition(decl->getType());

            // FIXME: ParamVarDecl
        }
        return true;
    }

    // FIXME: check templated decl.
    bool VisitFunctionDecl(const clang::FunctionDecl* decl) {
        if(Location location = indexer.toLocation(decl->getLocation())) {
            auto symbol = indexer.addSymbol(decl);
            symbol.addOccurrence(location);
            symbol.addDeclarationOrDefinition(decl->isThisDeclarationADefinition(), location);
            if(auto CMD = llvm::dyn_cast<clang::CXXMethodDecl>(decl)) {
                // FIXME: virtual override const ...

                if(auto CCD = llvm::dyn_cast<clang::CXXConstructorDecl>(CMD)) {
                    symbol.addTypeDefinition(CCD->getThisType());
                }

                // FIXME: CXXConversionDecl, CXXDestructorDecl
            }

            // FIXME: CXXDeductionGuideDecl
        }
        currentFunction = decl;
        return true;
    }

    /// FIXME: how to resolve shadow decls? a name could refer to multiple decls.
    /// Don't need to handle UsingEnumDecl, it's handled by VisitTypeLoc.
    bool VisitUsingDecl(const clang::UsingDecl* decl) {
        /// `using Foo::bar`
        ///             ^^^~~~ reference
        // if(Location location = indexer.toLocation(decl->getLocation())) {
        //     auto symbol = indexer.addSymbol(decl);
        //     symbol.addOccurrence(location);
        //     symbol.addReference(location);
        // }
        return true;
    }

    bool VisitBindingDecl(const clang::BindingDecl* decl) {
        /// `auto [a, b] = std::make_pair(1, 2);`
        ///        ^~~~ definition
        if(Location location = indexer.toLocation(decl->getLocation())) {
            auto symbol = indexer.addSymbol(decl);
            symbol.addOccurrence(location);
            symbol.addDefinition(location);
            symbol.addTypeDefinition(decl->getType());
        }
        return true;
    }

    bool VisitClassTemplateDecl(const clang::ClassTemplateDecl* decl) {
        auto name = decl->getDeclName();
        return true;
    }

    bool VisitConceptDecl(const clang::ConceptDecl* decl) {
        if(Location location = indexer.toLocation(decl->getLocation())) {
            auto symbol = indexer.addSymbol(decl);
            symbol.addOccurrence(location);
            symbol.addDefinition(location);
        }
        return true;
    }

    /// ============================================================================
    ///                                 Statement
    /// ============================================================================

    bool VisitDeclRefExpr(const clang::DeclRefExpr* expr) {
        Location location = indexer.toLocation(expr->getLocation());
        auto symbol = indexer.addSymbol(expr->getDecl());
        symbol.addOccurrence(location);
        symbol.addReference(location);
        return true;
    }

    bool VisitMemberExpr(const clang::MemberExpr* expr) {
        Location location = indexer.toLocation(expr->getMemberLoc());
        auto symbol = indexer.addSymbol(expr->getMemberDecl());
        symbol.addOccurrence(location);
        symbol.addReference(location);
        return true;
    }

    // FIXME: modify template resolver to cache recursively resolved results.
    bool VisitDependentScopeDeclRefExpr(const clang::DependentScopeDeclRefExpr* expr) {
        return true;
    }

    bool VisitCXXDependentScopeMemberExpr(const clang::CXXDependentScopeMemberExpr* expr) {
        return true;
    }

    bool VisitCallExpr(const clang::CallExpr* expr) {
        const clang::NamedDecl* caller = currentFunction;
        auto callee = llvm::dyn_cast<clang::NamedDecl>(expr->getCalleeDecl());
        if(caller && callee) {
            Location location = indexer.toLocation(expr->getSourceRange());
            auto symbol = indexer.addSymbol(caller);
            symbol.addCall(location, callee);
        } else {
            std::terminate();
        }
        return true;
    }

    /// ============================================================================
    ///                                  TypeLoc
    /// ============================================================================

    constexpr bool TraverseType [[gnu::const]] (clang::QualType) {
        return true;
    }

    /// FIXME: Render keyword in source, like `struct`, `class`, `union`, `enum`, `typename`,
    /// in `BuiltinTypeLoc`, `ElaboratedTypeLoc` and so on.
    bool VisitTagTypeLoc(clang::TagTypeLoc loc) {
        if(Location location = indexer.toLocation(loc.getNameLoc())) {
            auto symbol = indexer.addSymbol(loc.getDecl());
            symbol.addOccurrence(location);
            symbol.addReference(location);
        }
        return true;
    }

    bool VisitTypedefTypeLoc(clang::TypedefTypeLoc loc) {
        if(Location location = indexer.toLocation(loc.getNameLoc())) {
            auto symbol = indexer.addSymbol(loc.getTypedefNameDecl());
            symbol.addOccurrence(location);
            symbol.addReference(location);
        }
        return true;
    }

    /// FIXME: see `VisitUsingDecl`.
    bool VisitUsingTypeLoc(clang::UsingTypeLoc loc) {
        return true;
    }

    /// Render template name in `TemplateSpecializationTypeLoc`.
    /// `std::vector<int>`
    ///        ^^^^~~~~ reference
    bool VisitTemplateSpecializationTypeLoc(clang::TemplateSpecializationTypeLoc loc) {
        Location location = indexer.toLocation(loc.getTemplateNameLoc());
        if(!location) {
            return true;
        }

        const clang::TemplateSpecializationType* TST = loc.getTypePtr();
        clang::TemplateName name = TST->getTemplateName();
        clang::TemplateDecl* decl = name.getAsTemplateDecl();

        /// For a template specialization type, the template name is possibly a `ClassTemplateDecl`
        ///  or a `TypeAliasTemplateDecl`.
        if(auto TATD = llvm::dyn_cast<clang::TypeAliasTemplateDecl>(decl)) {
            /// Beacuse type alias template is not allowed to have partial and full specialization,
            /// we just need to add reference to the primary template.
            auto symbol = indexer.addSymbol(TATD);
            symbol.addOccurrence(location);
            symbol.addReference(location);
            return true;
        }

        /// If it's not a `TypeAliasTemplateDecl`, it must be a `ClassTemplateDecl`.
        auto CTD = llvm::cast<clang::ClassTemplateDecl>(decl);

        /// If it's a dependent type, we only consider the primary template now.
        /// FIXME: When `TemplateSpecializationTypeLoc` occurs in `NestedNameSpecifierLoc`, use
        /// `TemplateResolver` to resolve the template.
        if(TST->isDependentType()) {
            auto symbol = indexer.addSymbol(CTD);
            symbol.addOccurrence(location);
            symbol.addReference(location);
            return true;
        }

        /// For non dependent types, it must has been instantiated(implicit or explicit).
        /// Find instantiated decl for it, primary, partial or full.
        void* pos;
        auto spec = CTD->findSpecialization(TST->template_arguments(), pos);
        assert(spec && "Invalid specialization");

        /// Full specialization.
        if(spec->isExplicitSpecialization()) {
            auto symbol = indexer.addSymbol(spec);
            symbol.addOccurrence(location);
            symbol.addReference(location);
            return true;
        }

        auto specialized = spec->getSpecializedTemplateOrPartial();
        assert(!specialized.isNull() && "Invalid specialization");

        /// Partial specialization.
        if(auto PSD = specialized.get<clang::ClassTemplatePartialSpecializationDecl*>()) {
            auto symbol = indexer.addSymbol(PSD);
            symbol.addOccurrence(location);
            symbol.addReference(location);
            return true;
        }

        /// Primary template.
        auto symbol = indexer.addSymbol(specialized.get<clang::ClassTemplateDecl*>());
        symbol.addOccurrence(location);
        symbol.addReference(location);
        return true;
    }

    // TODO: TemplateTypeParmTypeLoc, AttributedTypeLoc, MacroQualifiedTypeLoc, ParenType,
    // AdjustedTypeLoc MemberPointerTypeLoc

    constexpr bool TraverseNestedNameSpecifier [[gnu::const]] (clang::NestedNameSpecifier*) {
        return true;
    }

    bool TraverseNestedNameSpecifierLoc(clang::NestedNameSpecifierLoc NNS) {
        // FIXME: use TemplateResolver here.
        auto range = NNS.getSourceRange();
        auto range2 = NNS.getLocalSourceRange();
        return Base::TraverseNestedNameSpecifierLoc(NNS);
    }

    bool TraverseTemplateArgumentLoc(const clang::TemplateArgumentLoc& argument) {
        return Base::TraverseTemplateArgumentLoc(argument);
    }

    bool TraverseCXXBaseSpecifier(const clang::CXXBaseSpecifier& base) {
        return Base::TraverseCXXBaseSpecifier(base);
    }

    bool TraverseConstructorInitializer(clang::CXXCtorInitializer* init) {
        return Base::TraverseConstructorInitializer(init);
    }

private:
    IndexBuilder& indexer;
    clang::ASTContext& context;
    clang::SourceManager& srcMgr;
    const clang::FunctionDecl* currentFunction = nullptr;
};

memory::Index IndexBuilder::index() && {
    IndexCollector collector(*this, context);
    collector.TraverseAST(context);

    // FIXME: sort relations ?
    llvm::sort(result.symbols, [](const memory::Symbol& lhs, const memory::Symbol& rhs) {
        return refl::less(lhs.id, rhs.id);
    });

    llvm::sort(result.occurrences,
               [](const memory::Occurrence& lhs, const memory::Occurrence& rhs) {
                   return lhs.location < rhs.location;
               });

    return std::move(result);
}

}  // namespace

memory::Index index(Compiler& compiler) {
    IndexBuilder builder(compiler.sema(), compiler.tokBuf());
    return std::move(builder).index();
}

}  // namespace clice::index
