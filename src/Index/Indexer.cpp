#include <clang/Index/USRGeneration.h>
#include <Support/FileSystem.h>
#include <Index/Indexer.h>
#include <Support/Reflection.h>

namespace clice::index {

namespace {

/// The `Indexer` is used to collect data from the AST and generate an index.
class IndexBuilder {
public:
    IndexBuilder(clang::Sema& sema) :
        sema(sema), context(sema.getASTContext()), srcMgr(context.getSourceManager()) {}

    Location toLocation(clang::SourceRange range) {
        Location location = {};
        auto begin = range.getBegin();
        auto end = range.getEnd();

        if(begin.isInvalid() && end.isInvalid()) {
            return location;
        }

        // if(range.isValid() && srcMgr.getIncludeLoc(srcMgr.getFileID(begin)).isInvalid()) {
        //     if(srcMgr.getFileID(begin) != srcMgr.getMainFileID()) {
        //         begin.dump(srcMgr);
        //         srcMgr.getIncludeLoc(srcMgr.getFileID(begin)).dump(srcMgr);
        //         llvm::outs() << "Invalid range\n";
        //     }
        // }

        // FIXME: position encoding ?
        location.begin.line = srcMgr.getPresumedLineNumber(begin);
        location.begin.column = srcMgr.getPresumedColumnNumber(begin);
        location.end.line = srcMgr.getPresumedLineNumber(end);
        location.end.column = srcMgr.getPresumedColumnNumber(end);

        auto& files = result.files;
        auto id = srcMgr.getFileID(begin);
        auto index = files.size();
        auto [iter, success] = fileCache.try_emplace(id, index);
        location.file = iter->second;

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
            }
        }

        return location;
    }

    static memory::SymbolID generateSymbolID(const clang::Decl* decl) {
        llvm::SmallString<128> USR;
        clang::index::generateUSRForDecl(decl, USR);
        return memory::SymbolID{llvm::hash_value(USR), USR.str().str()};
    }

    /// Add a symbol to the index.
    memory::Symbol& addSymbol(const clang::NamedDecl* decl) {
        auto& symbols = result.symbols;
        auto canonical = decl->getCanonicalDecl();
        auto iter = symbolCache.find(canonical);
        if(iter != symbolCache.end()) {
            return symbols[iter->second];
        }

        symbolCache.try_emplace(canonical, symbols.size());

        memory::Symbol& symbol = symbols.emplace_back();
        symbol.id = generateSymbolID(canonical);
        symbol.name = decl->getNameAsString();
        return symbol;
    }

    IndexBuilder& addRelation(clang::NamedDecl* decl,
                              clang::SourceLocation,
                              std::initializer_list<RelationKind>) {
        return *this;
    }

    IndexBuilder& addOccurrence(const clang::NamedDecl* decl, clang::SourceRange range) {
        auto location = toLocation(range);
        if(location.isValid()) {
            memory::Symbol& symbol = addSymbol(decl);
            auto& occurrences = result.occurrences;
            occurrences.emplace_back(location, symbol.id);
        }
        return *this;
    }

    memory::Index index() &&;

private:
    clang::Sema& sema;
    clang::ASTContext& context;
    clang::SourceManager& srcMgr;
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

    constexpr bool TraverseType [[gnu::const]] (clang::QualType) {
        return true;
    }

    constexpr bool TraverseNestedNameSpecifier [[gnu::const]] (clang::NestedNameSpecifier*) {
        return true;
    }

    bool TraverseTypeLoc(clang::TypeLoc loc) {
        /// clang currently doesn't record any information for `QualifiedTypeLoc`.
        /// It has same location with its inner type. So we just ignore it.
        if(auto QTL = loc.getAs<clang::QualifiedTypeLoc>()) {
            return TraverseTypeLoc(QTL.getUnqualifiedLoc());
        }

        return Base::TraverseTypeLoc(loc);
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

    bool VisiDeclRefExpr(const clang::DeclRefExpr* expr) {
        auto decl = expr->getDecl();

        auto range = expr->getNameInfo().getSourceRange();
        // TODO: add occurrence.
        return true;
    }

    bool VisitMemberExpr(const clang::MemberExpr* expr) {
        auto decl = expr->getMemberDecl();

        auto range = expr->getMemberLoc();
        return true;
    }

    bool VisitDependentCXXScopeMemberExpr(const clang::DependentScopeDeclRefExpr* expr) {
        // TODO: use TemplateResolver here.
        auto decl = expr->getQualifier();
        auto range = expr->getNameInfo().getSourceRange();
        return true;
    }

    bool TraverseNestedNameSpecifierLoc(clang::NestedNameSpecifierLoc NNS) {
        // FIXME: use TemplateResolver here.

        auto range = NNS.getSourceRange();
        auto range2 = NNS.getLocalSourceRange();

        return Base::TraverseNestedNameSpecifierLoc(NNS);
    }

#define VISIT_DECL(name) bool Visit##name(const clang::name* decl)
#define VISIT_TYOELOC(name) bool Visit##name(clang::name loc)

    VISIT_DECL(NamedDecl) {
        // Every NamedDecl has a name should have a symbol.
        // FIXME: add linkage information. when find information for external linkage,
        // We need to cross reference with other TUs.
        indexer.addSymbol(decl);

        // FIXME: For some declaration with relation, we need to resolve them separately.
        // e.g. ClassTemplateSpecializationDecl <-> ClassTemplateDecl
        return true;
    }

    VISIT_DECL(VarDecl) {
        // llvm::outs() << "-------------------------\n";
        // decl->getTypeSourceInfo()->getTypeLoc().dump();
        return true;
    }

    // TODO: ... add occurrence and relation.

    VISIT_TYOELOC(BuiltinTypeLoc) {
        // FIXME: ....
        // possible multiple tokens, ... map them to BuiltinKind.
        auto range = loc.getSourceRange();
        return true;
    }

    bool VisitTagTypeLoc(clang ::TagTypeLoc loc) {
        auto decl = loc.getTypePtr()->getDecl();
        auto location = loc.getNameLoc();
        indexer.addOccurrence(decl, location)
            .addRelation(decl, location, {RelationKind::Reference});
        return true;
    }

    bool VisitElaboratedTypeLoc(clang ::ElaboratedTypeLoc loc) {
        // FIXME: check the keyword.
        auto keywordLoc = loc.getElaboratedKeywordLoc();
        switch(loc.getTypePtr()->getKeyword()) {
            case clang::ElaboratedTypeKeyword::Struct:
            case clang::ElaboratedTypeKeyword::Class:
            case clang::ElaboratedTypeKeyword::Union:
            case clang::ElaboratedTypeKeyword::Enum: {
                // indexer.addOccurrence(BuiltinSymbolKind::elaborated_type_specifier, keywordLoc);
            }

            case clang::ElaboratedTypeKeyword::Typename: {
            }

            case clang::ElaboratedTypeKeyword::None:
            case clang::ElaboratedTypeKeyword::Interface: {
            }
        };
        return true;
    }

    bool VisitTypedefTypeLoc(clang::TypedefTypeLoc loc) {
        auto decl = loc.getTypePtr()->getDecl();
        auto location = loc.getNameLoc();
        indexer.addOccurrence(decl, location)
            .addRelation(decl, location, {RelationKind::Reference});
        return true;
    }

    bool VisitUsingTypeLoc(clang::UsingTypeLoc loc) {
        auto decl = loc.getTypePtr()->getFoundDecl();
        auto location = loc.getNameLoc();
        indexer.addOccurrence(decl, location)
            .addRelation(decl, location, {RelationKind::Reference});
        return true;
    }

    bool VisitClassTemplateDecl(const clang::ClassTemplateDecl* decl) {
        auto name = decl->getDeclName();
        return true;
    }

    bool VisitTemplateSpecializationTypeLoc(clang::TemplateSpecializationTypeLoc loc) {
        auto nameLoc = loc.getTemplateNameLoc();
        const clang::TemplateSpecializationType* TST = loc.getTypePtr();
        clang::TemplateName name = TST->getTemplateName();
        clang::TemplateDecl* decl = name.getAsTemplateDecl();

        // FIXME: record relation.

        // For a template specialization type, the template name is possibly a ClassTemplateDecl or
        // a TypeAliasTemplateDecl.
        if(auto CTD = llvm::dyn_cast<clang::ClassTemplateDecl>(decl)) {
            // Dependent types are all handled in `TraverseNestedNameSpecifierLoc`.
            if(TST->isDependentType()) {
                return true;
            }

            // For non dependent types, it must has been instantiated(implicit or explicit).
            // Find instantiated decl for it, main, partial specialization, full specialization?.
            void* pos;
            if(auto spec = CTD->findSpecialization(TST->template_arguments(), pos)) {
                // If it's not full(explicit) specialization, find the primary template.
                if(!spec->isExplicitInstantiationOrSpecialization()) {
                    auto specialized = spec->getSpecializedTemplateOrPartial();
                    if(specialized.is<clang::ClassTemplateDecl*>()) {
                        indexer.addOccurrence(CTD, nameLoc)
                            .addRelation(
                                CTD,
                                nameLoc,
                                {RelationKind::Reference, RelationKind::ImplicitInstantiation});
                    } else {
                        auto PSD =
                            specialized.get<clang::ClassTemplatePartialSpecializationDecl*>();
                        indexer.addOccurrence(PSD, nameLoc)
                            .addRelation(
                                PSD,
                                nameLoc,
                                {RelationKind::Reference, RelationKind::ImplicitInstantiation})
                            .addRelation(CTD, nameLoc, {RelationKind::Reference});
                    }
                } else {
                    // full specialization
                    indexer.addOccurrence(spec, nameLoc)
                        .addRelation(spec,
                                     nameLoc,
                                     {RelationKind::Reference, RelationKind::FullSpecialization});
                }
            }
        } else if(auto TATD = llvm::dyn_cast<clang::TypeAliasTemplateDecl>(decl)) {
            // Beacuse type alias template is not allowed to have partial and full specialization,
            // So we do notin
            indexer.addOccurrence(TATD, nameLoc)
                .addRelation(TATD,
                             nameLoc,
                             {RelationKind::Reference, RelationKind::ImplicitInstantiation});
        }
        return true;
    }

    // TODO. TemplateTypeParmTypeLoc, AttributedTypeLoc, MacroQualifiedTypeLoc, ParenType,
    // AdjustedTypeLoc MemberPointerTypeLoc

private:
    IndexBuilder& indexer;
    clang::ASTContext& context;
    clang::SourceManager& srcMgr;
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

memory::Index index(clang::Sema& sema) {
    IndexBuilder builder(sema);
    return std::move(builder).index();
}

}  // namespace clice::index
