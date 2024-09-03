#pragma once

#include "ParsedAST.h"
#include <stack>

namespace clice {

/// This class is used to resolve dependent names in the AST.
/// For dependent names, we cannot know the any information about the name until the template is instantiated.
/// This can be frustrating, you cannot get completion, you cannot get go-to-definition, etc.
/// To avoid this, we just use some heuristics to simplify the dependent names as normal type/expression.
/// For example, `std::vector<T>::value_type` can be simplified as `T`.
class DependentNameResolver {
public:
    DependentNameResolver(clang::Sema& sema, clang::ASTContext& context) : sema(sema), context(context) {}

    clang::QualType simplify(clang::DependentNameType type);

    clang::QualType simplify(clang::DependentTemplateSpecializationType type);

    clang::ExprResult simplify(clang::DependentScopeDeclRefExpr* expr);

    clang::ExprResult simplify(clang::CXXDependentScopeMemberExpr* expr);

    clang::ExprResult simplify(clang::UnresolvedLookupExpr* expr);

    clang::DeclResult simplify(clang::UnresolvedUsingValueDecl* decl);

    clang::DeclResult simplify(clang::UnresolvedUsingTypenameDecl* decl);

    clang::ExprResult simplify(clang::CXXUnresolvedConstructExpr* expr);

private:
    clang::Type* lookup(const clang::CXXRecordDecl* CRD, const clang::IdentifierInfo* II) {
        auto reuslt = CRD->lookup(II);

        // FIXME: currently, we assume there are no member template specialization
        // that is the size of the partial specialization is 1,
        for(auto member: reuslt) {
            if(auto type = simplify(member)) {
                return type;
            }
        }

        for(auto base: CRD->bases()) {
            if(auto type = simplify(base.getType())) {
                return type;
            }
        }

        return nullptr;
    }

    void simplify(clang::NestedNameSpecifier* NNS, clang::IdentifierInfo* II) {
        switch(NNS->getKind()) {
            // when NNS is a type, e.g., `std::vector<T>::` or `T::`
            case clang::NestedNameSpecifier::TypeSpec: {
                auto type = NNS->getAsType();

                if(auto TST = type->getAs<clang::TemplateSpecializationType>()) {
                    auto TD = TST->getTemplateName().getAsTemplateDecl();
                    clang::QualType type;
                    if(auto CTD = llvm::dyn_cast<clang::ClassTemplateDecl>(TD)) {
                        // `std::vector<T>::`
                    } else if(auto TATD = llvm::dyn_cast<clang::TypeAliasTemplateDecl>(TD)) {
                        // template<typename T>
                        // using Vector = std::vector<T>::value_type;
                        // `Vector<T>::`
                    } else if(auto TTPD = llvm::dyn_cast<clang::TemplateTemplateParmDecl>(TD)) {
                        // template<typename T, template<typename> typename List>
                        // using X = List<T>::value_type;
                        // `List<T>::`
                    }

                } else if(auto TTPT = type->getAs<clang::TemplateTypeParmType>()) {
                    // `T::`
                }
            }
            case clang::NestedNameSpecifier::TypeSpecWithTemplate: {
                // TODO:
            }

            // when NNS is still a dependent name, e.g., `std::vector<T>::value_type::`
            case clang::NestedNameSpecifier::Identifier: {
                // TODO:
                {
                    auto prefix = NNS->getPrefix();
                    auto name = NNS->getAsIdentifier();
                }
            }
        }
    }

    clang::Type* simplify(clang::QualType);

    clang::Type* simplify(const clang::NamedDecl* ND) {}

    clang::Type* simplify(const clang::TemplateSpecializationType* TST, const clang::IdentifierInfo* II) {
        auto TD = TST->getTemplateName().getAsTemplateDecl();
        auto arguments = TST->template_arguments();

        if(auto CTD = llvm::dyn_cast<clang::ClassTemplateDecl>(TD)) {
            // `std::vector<T>::`

            // iterate all partial specializations
            llvm::SmallVector<clang::ClassTemplatePartialSpecializationDecl*> partials;
            CTD->getPartialSpecializations(partials);

            clang::sema::TemplateDeductionInfo Info(CTD->getLocation());
            for(auto partial: partials) {
                if(auto error = sema.DeduceTemplateArguments(partial, arguments, Info);
                   error == clang::TemplateDeductionResult::Success) {
                    if(auto result = lookup(partial, II)) {
                        return result;
                    }
                }
            }

            // fallback to main template
            if(auto result = lookup(CTD->getTemplatedDecl(), II)) {
                return result;
            }

        } else if(auto TATD = llvm::dyn_cast<clang::TypeAliasTemplateDecl>(TD)) {
            // template<typename T>
            // using Vector = std::vector<T>::value_type;
            // `Vector<T>::`
        } else if(auto TTPD = llvm::dyn_cast<clang::TemplateTemplateParmDecl>(TD)) {
            // template<typename T, template<typename> typename List>
            // using X = List<T>::value_type;
            // `List<T>::`
        }
    }

private:
    clang::Sema& sema;
    clang::ASTContext& context;
    std::stack<clang::TemplateDecl*> templateStack;
    std::stack<llvm::SmallVector<clang::TemplateArgument>> argumentsStack;
};

}  // namespace clice
