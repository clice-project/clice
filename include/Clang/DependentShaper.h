#pragma once

#include <Clang/Clang.h>

namespace clice {

class DependentShaper {
    clang::ASTContext& context;

public:
    DependentShaper(clang::ASTContext& context) : context(context) {}

    const clang::NamedDecl* simplify(const clang::TemplateSpecializationType* templateType,
                                     const clang::IdentifierInfo* identifier) {
        // get the template decl of the template specialization
        // e.g. vector<int> -> vector

        // get the template arguments of the template specialization
        // e.g. vector<int> -> int
        auto templateDecl = templateType->getTemplateName().getAsTemplateDecl();
        if(auto decl = llvm::dyn_cast<clang::ClassTemplateDecl>(templateDecl)) {
            // get the record decl of the template decl
            clang::CXXRecordDecl* recordDecl = decl->getTemplatedDecl();
            // lookup the identifier in the record decl
            auto result = recordDecl->lookup(identifier);
            assert(result.size() == 1 && "multiple results found");
            return result.front();

        } else if(auto decl = llvm::dyn_cast<clang::TypeAliasTemplateDecl>(templateDecl)) {
        }
    }

    const clang::NamedDecl* simplify(const clang::NestedNameSpecifier* specifier,
                                     const clang::IdentifierInfo* identifier) {
        switch(specifier->getKind()) {
            case clang::NestedNameSpecifier::TypeSpec: {
                auto node = specifier->getAsType();

                if(auto type = node->getAs<clang::TemplateTypeParmType>()) {
                    // represent a direct dependent name, e.g. typename T::^ name
                    // and can not be further simplified
                } else if(auto type = node->getAs<clang::TemplateSpecializationType>()) {
                    // represent a dependent name that is a template specialization
                    // e.g. typename vector<int>::^ name, and can be further simplified
                    return simplify(type, identifier);
                }

                break;
            }

            case clang::NestedNameSpecifier::TypeSpecWithTemplate: {
                break;
            }

            default: {
            }
        }
    }

    const clang::NamedDecl* simplify(const clang::DependentNameType* type) {
        return simplify(type->getQualifier(), type->getIdentifier());
    }
};

}  // namespace clice
