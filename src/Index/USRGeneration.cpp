#include "clang/AST/DeclVisitor.h"

namespace clice::index {

namespace {

class USRGenerator : public clang::ConstDeclVisitor<USRGenerator> {
public:
    USRGenerator(llvm::SmallVectorImpl<char>& buffer,
                 const clang::ASTContext& Ctx,
                 const clang::SourceManager& SM) : buffer(buffer), Ctx(Ctx), SM(SM) {}

    void VisitDeclContext(const clang::DeclContext* DC) {
        if(auto ND = llvm::dyn_cast<clang::NamedDecl>(DC)) {
            Visit(ND);
        } else if(auto LSD = llvm::dyn_cast<clang::LinkageSpecDecl>(DC)) {
            VisitDeclContext(DC->getParent());
        }
    }

#define VISIT_DECL(Type) void Visit##Type(const clang::Type* decl)

    VISIT_DECL(NamespaceDecl) {}

    VISIT_DECL(NamespaceAliasDecl) {}

    VISIT_DECL(FieldDecl) {}

    VISIT_DECL(EnumConstantDecl) {}

    VISIT_DECL(VarDecl) {}

    VISIT_DECL(VarTemplateDecl) {}

    VISIT_DECL(VarTemplateSpecializationDecl) {}

    VISIT_DECL(VarTemplatePartialSpecializationDecl) {}

    VISIT_DECL(FunctionDecl) {}

    VISIT_DECL(FunctionTemplateDecl) {}

    VISIT_DECL(RecordDecl) {}

    VISIT_DECL(CXXRecordDecl) {}

    VISIT_DECL(ClassTemplateDecl) {}

    VISIT_DECL(ClassTemplateSpecializationDecl) {}

    VISIT_DECL(ClassTemplatePartialSpecializationDecl) {}

#undef VISIT_DECL

private:
    llvm::SmallVectorImpl<char>& buffer;

    const clang::ASTContext& Ctx;
    const clang::SourceManager& SM;
};

}  // namespace

}  // namespace clice::index
