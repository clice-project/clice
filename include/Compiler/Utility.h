#pragma once

#include <clang/Basic/SourceLocation.h>
#include <clang/Basic/SourceManager.h>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/ASTContext.h>

namespace clice {

inline bool is_inside_main_file(clang::SourceLocation Loc, const clang::SourceManager& SM) {
    if(!Loc.isValid())
        return false;
    clang::FileID FID = SM.getFileID(SM.getExpansionLoc(Loc));
    return FID == SM.getMainFileID() || FID == SM.getPreambleFileID();
}

template <class T>
bool isTemplateSpecializationKind(const clang::NamedDecl* D,
                                  clang::TemplateSpecializationKind Kind) {
    if(const auto* TD = dyn_cast<T>(D))
        return TD->getTemplateSpecializationKind() == Kind;
    return false;
}

inline bool isTemplateSpecializationKind(const clang::NamedDecl* D,
                                         clang::TemplateSpecializationKind Kind) {
    return isTemplateSpecializationKind<clang::FunctionDecl>(D, Kind) ||
           isTemplateSpecializationKind<clang::CXXRecordDecl>(D, Kind) ||
           isTemplateSpecializationKind<clang::VarDecl>(D, Kind);
}

inline bool isImplicitTemplateInstantiation(const clang::NamedDecl* D) {
    return isTemplateSpecializationKind(D, clang::TSK_ImplicitInstantiation);
}

inline bool isClangdTopLevelDecl(const clang::Decl* D) {
    auto& SM = D->getASTContext().getSourceManager();
    if(!is_inside_main_file(D->getLocation(), SM))
        return false;
    if(const clang::NamedDecl* ND = dyn_cast<clang::NamedDecl>(D))
        if(isImplicitTemplateInstantiation(ND))
            return false;

    // ObjCMethodDecl are not actually top-level decls.
    if(isa<clang::ObjCMethodDecl>(D))
        return false;
    return true;
}

}  // namespace clice
