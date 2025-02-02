#include <Compiler/Utility.h>

namespace clice {

bool isDefinition(const clang::Decl* decl) {
    if(auto VD = llvm::dyn_cast<clang::VarDecl>(decl)) {
        return VD->isThisDeclarationADefinition();
    }

    if(auto FD = llvm::dyn_cast<clang::FunctionDecl>(decl)) {
        return FD->isThisDeclarationADefinition();
    }

    if(auto TD = llvm::dyn_cast<clang::TagDecl>(decl)) {
        return TD->isThisDeclarationADefinition();
    }

    if(llvm::isa<clang::FieldDecl,
                 clang::EnumConstantDecl,
                 clang::TypedefNameDecl,
                 clang::ConceptDecl>(decl)) {
        return true;
    }

    return false;
}

const static clang::CXXRecordDecl* getDeclContextForTemplateInstationPattern(const clang::Decl* D) {
    if(const auto* CTSD = dyn_cast<clang::ClassTemplateSpecializationDecl>(D->getDeclContext())) {
        return CTSD->getTemplateInstantiationPattern();
    }

    if(const auto* RD = dyn_cast<clang::CXXRecordDecl>(D->getDeclContext())) {
        return RD->getInstantiatedFromMemberClass();
    }

    return nullptr;
}

const clang::NamedDecl* instantiatedFrom(const clang::NamedDecl* decl) {
    if(auto CTSD = llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(decl)) {

        auto kind = CTSD->getTemplateSpecializationKind();
        if(kind == clang::TSK_Undeclared) {
            /// The instantiation of template is lazy, in this case, the specialization is undeclared.
            /// Temporarily return primary template of the specialization.
            /// FIXME: Is there a better way to handle such case?
            return CTSD->getSpecializedTemplate()->getTemplatedDecl();
        } else if(kind == clang::TSK_ExplicitSpecialization) {
            /// If the decl is an full specialization, return itself.
            return CTSD;
        }

        return CTSD->getTemplateInstantiationPattern();
    }

    if(auto FD = llvm::dyn_cast<clang::FunctionDecl>(decl)) {
        /// If the decl is an full specialization, return itself.
        if(FD->getTemplateSpecializationKind() == clang::TSK_ExplicitSpecialization) {
            return FD;
        }

        return FD->getTemplateInstantiationPattern();
    }

    if(auto VD = llvm::dyn_cast<clang::VarDecl>(decl)) {
        /// If the decl is an full specialization, return itself.
        if(VD->getTemplateSpecializationKind() == clang::TSK_ExplicitSpecialization) {
            return VD;
        }

        return VD->getTemplateInstantiationPattern();
    }

    if(auto CRD = llvm::dyn_cast<clang::CXXRecordDecl>(decl)) {
        return CRD->getInstantiatedFromMemberClass();
    }

    /// For `FieldDecl` and `TypedefNameDecl`, clang will not store their instantiation information
    /// in the AST. So we need to look up the original decl manually.
    if(llvm::isa<clang::FieldDecl, clang::TypedefNameDecl>(decl)) {
        /// FIXME: figure out the context.
        if(auto context = getDeclContextForTemplateInstationPattern(decl)) {
            for(auto member: context->lookup(decl->getDeclName())) {
                if(member->isImplicit()) {
                    continue;
                }

                if(member->getKind() == decl->getKind()) {
                    return member;
                }
            }
        }
    }

    if(auto ED = llvm::dyn_cast<clang::EnumDecl>(decl)) {
        return ED->getInstantiatedFromMemberEnum();
    }

    if(auto ECD = llvm::dyn_cast<clang::EnumConstantDecl>(decl)) {
        auto ED = llvm::cast<clang::EnumDecl>(ECD->getDeclContext());
        if(auto context = ED->getInstantiatedFromMemberEnum()) {
            for(auto member: context->lookup(ECD->getDeclName())) {
                return member;
            }
        }
    }

    return nullptr;
}

clang::QualType typeForDecl(const clang::NamedDecl* decl) {
    if(auto VD = llvm::dyn_cast<clang::VarDecl>(decl)) {
        return VD->getType();
    }

    if(auto FD = llvm::dyn_cast<clang::FieldDecl>(decl)) {
        return FD->getType();
    }

    if(auto ECD = llvm::dyn_cast<clang::EnumConstantDecl>(decl)) {
        return ECD->getType();
    }

    if(auto BD = llvm::dyn_cast<clang::BindingDecl>(decl)) {
        return BD->getType();
    }

    if(auto TD = llvm::dyn_cast<clang::TypedefNameDecl>(decl)) {
        return TD->getUnderlyingType();
    }

    if(auto CCD = llvm::dyn_cast<clang::CXXConstructorDecl>(decl)) {
        return CCD->getThisType();
    }

    if(auto CDD = llvm::dyn_cast<clang::CXXDestructorDecl>(decl)) {
        return CDD->getThisType();
    }

    return clang::QualType();
}

const clang::NamedDecl* declForType(clang::QualType type) {
    if(type.isNull()) {
        return nullptr;
    }

    if(auto RT = type->getAs<clang::TagType>()) {
        return RT->getDecl();
    }

    if(auto TT = type->getAs<clang::TagType>()) {
        return TT->getDecl();
    }

    /// FIXME:
    if(auto TST = type->getAs<clang::TemplateSpecializationType>()) {
        auto decl = TST->getTemplateName().getAsTemplateDecl();
        if(type->isDependentType()) {
            return decl;
        }

        /// For a template specialization type, the template name is possibly a `ClassTemplateDecl`
        ///  `TypeAliasTemplateDecl` or `TemplateTemplateParmDecl` and `BuiltinTemplateDecl`.
        if(llvm::isa<clang::TypeAliasTemplateDecl>(decl)) {
            return decl->getTemplatedDecl();
        }

        if(llvm::isa<clang::TemplateTemplateParmDecl,

                     clang::BuiltinTemplateDecl>(decl)) {
            return decl;
        }

        return instantiatedFrom(TST->getAsCXXRecordDecl());
    }

    return nullptr;
}

void dumpInclude(clang::SourceManager& srcMgr, clang::SourceLocation loc) {
    auto file = srcMgr.getFileID(loc);
    for(auto includeLoc = srcMgr.getIncludeLoc(file); includeLoc.isValid();
        includeLoc = srcMgr.getIncludeLoc(file)) {
        includeLoc.dump(srcMgr);
        file = srcMgr.getFileID(includeLoc);
    }
}
}  // namespace clice
