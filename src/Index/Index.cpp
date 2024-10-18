#include <Index/SymbolSlab.h>

namespace clice {

namespace {

class SymbolCollector : public clang::RecursiveASTVisitor<SymbolCollector> {
    using Base = clang::RecursiveASTVisitor<SymbolCollector>;

public:
    SymbolCollector(SymbolSlab& slab, clang::ASTContext& context) :
        slab(slab), context(context), srcMgr(context.getSourceManager()) {}

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

    // TODO: ... add occurrence and relation.

    VISIT_TYOELOC(BuiltinTypeLoc) {
        // FIXME: ....
        // possible multiple tokens, ... map them to BuiltinKind.
        auto range = loc.getSourceRange();
        return true;
    }

    VISIT_TYOELOC(RecordTypeLoc) {
        auto range = loc.getSourceRange();
        return true;
    }

    VISIT_TYOELOC(EnumTypeLoc) {
        auto range = loc.getSourceRange();
        return true;
    }

    VISIT_TYOELOC(TypedefTypeLoc) {
        auto range = loc.getSourceRange();
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
                slab.addOccurrence(BuiltinSymbolKind::elaborated_type_specifier, keywordLoc);
            }

            case clang::ElaboratedTypeKeyword::Typename: {
            }

            case clang::ElaboratedTypeKeyword::None:
            case clang::ElaboratedTypeKeyword::Interface: {
            }
        };
        return true;
    }

    VISIT_DECL(NamedDecl) {
        // Every NamedDecl has a name should have a symbol.
        // FIXME: add linkage information. when find information for external linkage,
        // We need to cross reference with other TUs.
        slab.addSymbol(decl);

        // FIXME: For some declaration with relation, we need to resolve them separately.
        // e.g. ClassTemplateSpecializationDecl <-> ClassTemplateDecl
        return true;
    }

    VISIT_DECL(ClassTemplateDecl) {
        auto name = decl->getDeclName();
        return true;
    }

    bool VisitTemplateSpecializationTypeLoc(clang::TemplateSpecializationTypeLoc loc) {
        auto nameLoc = loc.getTemplateNameLoc();
        const clang::TemplateSpecializationType* TST = loc.getTypePtr();
        clang::TemplateName name = TST->getTemplateName();
        clang::TemplateDecl* decl = name.getAsTemplateDecl();

        // FIXME: record relation.

        // For a template specialization type, the template name is possibly a ClassTemplateDecl or a
        // TypeAliasTemplateDecl.
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
                    if(auto CTD = specialized.dyn_cast<clang::ClassTemplateDecl*>()) {
                        slab.addOccurrence(CTD, nameLoc, Role::ImplicitInstantiation);
                    } else {
                        auto PSD = specialized.get<clang::ClassTemplatePartialSpecializationDecl*>();
                        slab.addOccurrence(PSD, nameLoc, Role::ImplicitInstantiation);
                    }
                } else {
                    // full specialization
                    slab.addOccurrence(spec, nameLoc, Role::ExplicitInstantiation);
                }
            }
        } else if(auto TATD = llvm::dyn_cast<clang::TypeAliasTemplateDecl>(decl)) {
            // Beacuse type alias template is not allowed to have partial and full specialization,
            // So we do notin
            slab.addOccurrence(TATD, nameLoc, Role::ExplicitInstantiation);
        }
        return true;
    }

    // TODO. TemplateTypeParmTypeLoc, UsingType, AttributedTypeLoc, MacroQualifiedTypeLoc, ParenType, AdjustedTypeLoc
    // MemberPointerTypeLoc

private:
    SymbolSlab& slab;
    clang::ASTContext& context;
    clang::SourceManager& srcMgr;
};

}  // namespace

CSIF SymbolSlab::index() {
    CSIF csif;
    SymbolCollector collector(*this, sema.getASTContext());
    collector.TraverseAST(sema.getASTContext());

    csif.version = "0.1";
    csif.language = "C++";
    csif.symbols = symbols;
    csif.occurrences = occurrences;
    return csif;
};

}  // namespace clice
