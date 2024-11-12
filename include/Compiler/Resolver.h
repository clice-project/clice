#pragma once

#include <Compiler/Clang.h>

namespace clice {

/// This class is used to resolve dependent names in the AST.
/// For dependent names, we cannot know the any information about the name until
/// the template is instantiated. This can be frustrating, you cannot get
/// completion, you cannot get go-to-definition, etc. To avoid this, we just use
/// some heuristics to simplify the dependent names as normal type/expression.
/// For example, `std::vector<T>::value_type` can be simplified as `T`.
class TemplateResolver {
public:
    TemplateResolver(clang::Sema& sema) : sema(sema) {}

    clang::QualType resolve(clang::QualType type);

    clang::QualType resolve(const clang::DependentNameType* type) {
        if(auto iter = resolved.find(type); iter != resolved.end()) {
            return iter->second;
        }

        return resolve(clang::QualType(type, 0));
    }

    clang::QualType resolve(const clang::DependentTemplateSpecializationType* type) {
        if(auto iter = resolved.find(type); iter != resolved.end()) {
            return iter->second;
        }

        return resolve(clang::QualType(type, 0));
    }

    clang::lookup_result resolve(const clang::DependentScopeDeclRefExpr* expr);

    clang::ExprResult resolve(clang::CXXDependentScopeMemberExpr* expr);

    clang::ExprResult resolve(clang::CXXUnresolvedConstructExpr* expr);

    clang::ExprResult resolve(clang::UnresolvedLookupExpr* expr);

    // TODO: use a relative clear way to resolve `UnresolvedLookupExpr`.
    void resolve(clang::UnresolvedUsingValueDecl* decl);

    void resolve(clang::UnresolvedUsingTypenameDecl* decl);

    void resolve(clang::UnresolvedUsingType* type);

private:
    clang::Sema& sema;
    llvm::DenseMap<const void*, clang::QualType> resolved;
};

}  // namespace clice
