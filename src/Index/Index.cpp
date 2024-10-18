#include <Index/SymbolSlab.h>

namespace clice {

namespace {

class SymbolCollector : public clang::RecursiveASTVisitor<SymbolCollector> {
    using Base = clang::RecursiveASTVisitor<SymbolCollector>;

public:
    SymbolCollector(SymbolSlab& slab, clang::ASTContext& context) :
        slab(slab), context(context), srcMgr(context.getSourceManager()) {}

    bool TraverseDecl(clang::Decl* decl) {
        /// `TranslationUnitDecl` has invalid location information.
        /// So we process it separately.
        if(llvm::isa_and_nonnull<clang::TranslationUnitDecl>(decl)) {
            return Base::TraverseDecl(decl);
        }

        if(decl->isImplicit()) {
            return true;
        }

        slab.addSymbol(decl);
        llvm::outs() << "------------------------------------------\n";
        decl->dump();

        // TODO: generate SymbolID for every decl.
        // Distinguish linkage, for no or internal linkage.
        // For them, relation lookup is only occurred in current TU.

        return Base::TraverseDecl(decl);
    }

    // FIXME: check DeclRefExpr, MemberExpr, etc.

    bool TraverseStmt(clang::Stmt* stmt) {
        return Base::TraverseStmt(stmt);
    }

    bool TraverseAttr(clang::Attr* attr) {
        return Base::TraverseAttr(attr);
    }

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
        // TODO: use TemplateResolver here.
        auto range = NNS.getSourceRange();
        auto range2 = NNS.getLocalSourceRange();

        return Base::TraverseNestedNameSpecifierLoc(NNS);
    }

#define VISIT_TYOELOC(name) bool Visit##name(clang::name loc)

    // TODO: ... add occurrence and relation.

    VISIT_TYOELOC(BuiltinTypeLoc) {
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

    VISIT_TYOELOC(ElaboratedTypeLoc) {
        auto loc1 = loc.getElaboratedKeywordLoc();

        auto line = srcMgr.getPresumedLineNumber(loc1);
        auto column = srcMgr.getPresumedColumnNumber(loc1);

        switch(loc.getTypePtr()->getKeyword()) {
            case clang::ElaboratedTypeKeyword::Struct:
            case clang::ElaboratedTypeKeyword::Class:
            case clang::ElaboratedTypeKeyword::Union:
            case clang::ElaboratedTypeKeyword::Enum: {
                slab.addOccurrence(BuiltinSymbolKind::elaborated_type_specifier, {line, column});
            }

            case clang::ElaboratedTypeKeyword::Typename: {
            }

            case clang::ElaboratedTypeKeyword::None:
            case clang::ElaboratedTypeKeyword::Interface: {
            }
        };
        loc1.dump(srcMgr);
        return true;
        // render keyword.
    }

    VISIT_TYOELOC(TypedefTypeLoc) {
        auto range = loc.getSourceRange();
        return true;
    }

    VISIT_TYOELOC(TemplateSpecializationTypeLoc) {
        // TODO: add Relation.

        auto name = loc.getTypePtr()->getTemplateName().getUnderlying();
        auto decl = name.getAsTemplateDecl();
        if(auto CTD = llvm::dyn_cast<clang::ClassTemplateDecl>(decl)) {
            auto nameLoc = loc.getTemplateNameLoc();
            auto line = srcMgr.getPresumedLineNumber(nameLoc);
            auto column = srcMgr.getPresumedColumnNumber(nameLoc);
            proto::Range range = {line, column, line, static_cast<proto::uinteger>(column + decl->getName().size())};

            void* ptr = CTD;
            clang::ClassTemplateSpecializationDecl* decl2 =
                CTD->findSpecialization(loc.getTypePtr()->template_arguments(), ptr);
            auto main = decl2->getSpecializedTemplateOrPartial();

            if(main.is<clang::ClassTemplateDecl*>()) {
                auto decl3 = main.get<clang::ClassTemplateDecl*>();
                slab.addOccurrence(decl3, range, Role::ExplicitInstantiation);
            } else {
                auto decl3 = main.get<clang::ClassTemplatePartialSpecializationDecl*>();
                slab.addOccurrence(decl3, range, Role::ExplicitInstantiation);
            }
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

CSIF SymbolSlab::index(clang::Sema& sema, clang::syntax::TokenBuffer& tokBuf) {
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
