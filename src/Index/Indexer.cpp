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

    uint32_t toLocation(clang::SourceRange range) {
        auto begin = range.getBegin();
        auto end = range.getEnd();
        if(begin.isInvalid() || end.isInvalid() || begin.isMacroID() || end.isMacroID()) {
            return std::numeric_limits<uint32_t>::max();
        }

        auto [iter, success] = locationCache.try_emplace({begin, end}, result.locations.size());
        const auto index = iter->second;
        if(success) {
            result.locations.emplace_back();
            Location& location = result.locations.back();

            auto beginLoc = srcMgr.getPresumedLoc(begin);
            auto endLoc = srcMgr.getPresumedLoc(
                clang::Lexer::getLocForEndOfToken(end, 0, srcMgr, sema.getLangOpts()));

            /// FIXME: position encoding?
            location.begin.line = beginLoc.getLine();
            location.begin.column = beginLoc.getColumn();
            location.end.line = endLoc.getLine();
            location.end.column = endLoc.getColumn();
            location.file = addFile(beginLoc.getFileID());
        }
        return index;
    }

    friend class SymbolRef;

    struct SymbolRef {
        IndexBuilder& builder;
        uint32_t index;

        const uint64_t id() const {
            return builder.result.symbols[index].id;
        }

        SymbolRef addRelation(RelationKind kind, uint32_t location, uint32_t related = {}) {
            auto& relations = builder.result.symbols[index].relations;
            relations.emplace_back(static_cast<RelationKind>(kind), location, related);
            return *this;
        }

        SymbolRef addRelation(uint32_t kind, uint32_t location) {
            auto& relations = builder.result.symbols[index].relations;
            relations.emplace_back(static_cast<RelationKind>(kind), location);
            return *this;
        }

        SymbolRef addReference(uint32_t location) {
            return addRelation(RelationKind::Reference, location);
        }

        // A definition is also a declaration and a reference.
        SymbolRef addDefinition(uint32_t location) {
            return addRelation(RelationKind(RelationKind::Reference,
                                            RelationKind::Declaration,
                                            RelationKind::Definition),
                               location);
        }

        SymbolRef addDeclarationOrDefinition(bool is_definition, uint32_t location) {
            RelationKind kind = RelationKind(RelationKind::Reference, RelationKind::Declaration);
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

        SymbolRef addOccurrence(uint32_t location) {
            builder.result.occurrences.emplace_back(Occurrence{location, index});
            return *this;
        }

        /// Should be called by caller, add `Caller` and `Callee` relation between two symbols.
        SymbolRef addCall(uint32_t location, const clang::Decl* callee) {
            auto symbol = builder.addSymbol(llvm::dyn_cast<clang::NamedDecl>(callee));
            symbol.addRelation(RelationKind::Caller, location, id());
            return addRelation(RelationKind::Callee, location, symbol.id());
        }
    };

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
            llvm::SmallString<128> USR;
            clang::index::generateUSRForDecl(decl, USR);
            symbol.id = llvm::xxHash64(USR);
            symbol.USR = USR.str();
            auto info = decl->getLinkageAndVisibility();
            auto linkage = info.getLinkage();
            /// FIXME: figure out linkage.
            symbol.is_local = linkage == clang::Linkage::Internal;
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
    llvm::DenseMap<std::pair<clang::SourceLocation, clang::SourceLocation>, std::size_t>
        locationCache;
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
        /// `namespace Foo { }`
        ///             ^~~~ definition
        auto location = indexer.toLocation(decl->getLocation());
        auto symbol = indexer.addSymbol(decl);
        symbol.addOccurrence(location);
        symbol.addDefinition(location);
        return true;
    }

    bool VisitNamespaceAliasDecl(const clang::NamespaceAliasDecl* decl) {
        /// `namespace Foo = Bar`
        ///             ^     ^~~~ reference
        ///             ^~~~ definition
        auto location = indexer.toLocation(decl->getLocation());
        auto symbol = indexer.addSymbol(decl);
        symbol.addOccurrence(location);
        symbol.addDefinition(location);

        {
            auto location = indexer.toLocation(decl->getTargetNameLoc());
            auto symbol = indexer.addSymbol(decl->getNamespace());
            symbol.addOccurrence(location);
            symbol.addReference(location);
        }
        return true;
    }

    bool VisitUsingDirectiveDecl(const clang::UsingDirectiveDecl* decl) {
        /// `using namespace Foo`
        ///                  ^^^~~~~~~ reference
        auto location = indexer.toLocation(decl->getLocation());
        auto symbol = indexer.addSymbol(decl->getNominatedNamespace());
        symbol.addOccurrence(location);
        symbol.addReference(location);
        return true;
    }

    bool VisitLabelDecl(const clang::LabelDecl* decl) {
        /// `label:`
        ///    ^~~~ definition
        auto location = indexer.toLocation(decl->getLocation());
        auto symbol = indexer.addSymbol(decl);
        symbol.addOccurrence(location);
        symbol.addDefinition(location);
        return true;
    }

    bool VisitTagDecl(const clang::TagDecl* decl) {
        auto location = indexer.toLocation(decl->getLocation());
        auto symbol = indexer.addSymbol(decl);
        symbol.addOccurrence(location);
        symbol.addDefinition(location);
        return true;
    }

    bool VisitFieldDecl(const clang::FieldDecl* decl) {
        auto location = indexer.toLocation(decl->getLocation());
        auto symbol = indexer.addSymbol(decl);
        symbol.addOccurrence(location);
        symbol.addDefinition(location);
        symbol.addTypeDefinition(decl->getType());
        return true;
    }

    bool VisitEnumConstantDecl(const clang::EnumConstantDecl* decl) {
        auto location = indexer.toLocation(decl->getLocation());
        auto symbol = indexer.addSymbol(decl);
        symbol.addOccurrence(location);
        symbol.addDefinition(location);
        symbol.addTypeDefinition(decl->getType());
        return true;
    }

    /// `TypedefDecl` and `TypedefNameDecl` are both inherited from `TypedefNameDecl`.
    /// So we only need to handle `TypedefNameDecl`.
    /// FIXME: `TypeAliasTemplateDecl`.
    bool VisitTypedefNameDecl(const clang::TypedefNameDecl* TND) {
        auto location = indexer.toLocation(TND->getLocation());
        auto symbol = indexer.addSymbol(TND);
        symbol.addOccurrence(location);
        symbol.addDefinition(location);
        symbol.addTypeDefinition(TND->getUnderlyingType());
        return true;
    }

    bool VisitVarDecl(const clang::VarDecl* decl) {
        auto location = indexer.toLocation(decl->getLocation());
        auto symbol = indexer.addSymbol(decl);
        symbol.addOccurrence(location);
        symbol.addDeclarationOrDefinition(decl->isThisDeclarationADefinition(), location);
        symbol.addTypeDefinition(decl->getType());

        // FIXME: ParamVarDecl
        return true;
    }

    // FIXME: check templated decl.
    bool VisitFunctionDecl(const clang::FunctionDecl* decl) {
        auto location = indexer.toLocation(decl->getLocation());
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
        currentFunction = decl;
        return true;
    }

    /// FIXME: how to resolve shadow decls? a name could refer to multiple decls.
    /// Don't need to handle UsingEnumDecl, it's handled by VisitTypeLoc.
    bool VisitUsingDecl(const clang::UsingDecl* decl) {
        /// `using Foo::bar`
        ///             ^^^~~~ reference
        // if(auto location = indexer.toLocation(decl->getLocation())) {
        //     auto symbol = indexer.addSymbol(decl);
        //     symbol.addOccurrence(location);
        //     symbol.addReference(location);
        // }
        return true;
    }

    bool VisitBindingDecl(const clang::BindingDecl* decl) {
        /// `auto [a, b] = std::make_pair(1, 2);`
        ///        ^~~~ definition
        auto location = indexer.toLocation(decl->getLocation());
        auto symbol = indexer.addSymbol(decl);
        symbol.addOccurrence(location);
        symbol.addDefinition(location);
        symbol.addTypeDefinition(decl->getType());
        return true;
    }

    bool VisitClassTemplateDecl(const clang::ClassTemplateDecl* decl) {
        auto name = decl->getDeclName();
        return true;
    }

    bool VisitConceptDecl(const clang::ConceptDecl* decl) {
        if(auto location = indexer.toLocation(decl->getLocation())) {
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
        auto location = indexer.toLocation(expr->getLocation());
        auto symbol = indexer.addSymbol(expr->getDecl());
        symbol.addOccurrence(location);
        symbol.addReference(location);
        return true;
    }

    bool VisitMemberExpr(const clang::MemberExpr* expr) {
        auto location = indexer.toLocation(expr->getMemberLoc());
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

        /// FIXME: resolve dependent call expr.
        if(expr->isTypeDependent() || expr->isValueDependent() ||
           expr->isInstantiationDependent()) {
            return true;
        }

        auto callee = llvm::dyn_cast<clang::NamedDecl>(expr->getCalleeDecl());
        if(caller && callee) {
            auto location = indexer.toLocation(expr->getSourceRange());
            auto symbol = indexer.addSymbol(caller);
            symbol.addCall(location, callee);
            return true;
        }

        std::terminate();
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
        auto location = indexer.toLocation(loc.getNameLoc());
        auto symbol = indexer.addSymbol(loc.getDecl());
        symbol.addOccurrence(location);
        symbol.addReference(location);

        return true;
    }

    bool VisitTypedefTypeLoc(clang::TypedefTypeLoc loc) {
        auto location = indexer.toLocation(loc.getNameLoc());
        auto symbol = indexer.addSymbol(loc.getTypedefNameDecl());
        symbol.addOccurrence(location);
        symbol.addReference(location);
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
        auto location = indexer.toLocation(loc.getTemplateNameLoc());

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

static void sortSymbols(std::vector<memory::Symbol>& symbols,
                        std::vector<Occurrence>& occurrences) {
    const auto size = symbols.size();
    std::vector<uint32_t> indices;
    indices.reserve(size);
    for(std::size_t index = 0; index < size; index++) {
        indices.emplace_back(index);
    }

    llvm::sort(indices, [&](uint32_t lhs, uint32_t rhs) {
        return symbols[lhs].id < symbols[rhs].id;
    });

    std::vector<uint32_t> map;
    map.reserve(size);
    for(std::size_t index = 0; index < size; index++) {
        /// original index -> new index
        map[indices[index]] = index;
    }

    /// Adjust the index in occurrences and relations.
    for(auto& occurrence: occurrences) {
        occurrence.symbol = map[occurrence.symbol];
    }

    for(auto& symbol: symbols) {
        for(auto& relation: symbol.relations) {
            if(Relation::isSymbol(relation)) {
                relation.symOrLoc = map[relation.symOrLoc];
            }
        }
    }

    /// Sort symbols.
    llvm::sort(symbols, [](const memory::Symbol& lhs, const memory::Symbol& rhs) {
        return lhs.id < rhs.id;
    });
}

memory::Index IndexBuilder::index() && {
    IndexCollector collector(*this, context);
    collector.TraverseAST(context);

    // FIXME: sort relations ?

    /// Beacuse we store the index of symbols in Occurrence and Relation, Sorting symbols will
    /// invalidate the index. So we need to adjust the them. First, we need to obtain the index
    /// mapping before and after sorting.
    sortSymbols(result.symbols, result.occurrences);

    llvm::sort(result.occurrences, [&](const Occurrence& lhs, const Occurrence& rhs) {
        return result.locations[lhs.location] < result.locations[rhs.location];
    });

    return std::move(result);
}

}  // namespace

memory::Index index(Compiler& compiler) {
    IndexBuilder builder(compiler.sema(), compiler.tokBuf());
    return std::move(builder).index();
}

}  // namespace clice::index
