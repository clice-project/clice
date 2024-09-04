#pragma once

#include "ParsedAST.h"
#include <stack>
#include <clang/Sema/Lookup.h>
#include <clang/Sema/Template.h>

namespace clice {

/// This class is used to resolve dependent names in the AST.
/// For dependent names, we cannot know the any information about the name until the template is instantiated.
/// This can be frustrating, you cannot get completion, you cannot get go-to-definition, etc.
/// To avoid this, we just use some heuristics to simplify the dependent names as normal type/expression.
/// For example, `std::vector<T>::value_type` can be simplified as `T`.
class DependentNameResolver {
public:
    DependentNameResolver(clang::Sema& sema, clang::ASTContext& context) : sema(sema), context(context) {}

    std::vector<clang::TemplateArgument> resolve(llvm::ArrayRef<clang::TemplateArgument> arguments) {
        std::vector<clang::TemplateArgument> result;
        for(auto arg: arguments) {
            if(arg.getKind() == clang::TemplateArgument::ArgKind::Type) {
                // check whether it is a TemplateTypeParmType.
                if(auto type = llvm::dyn_cast<clang::TemplateTypeParmType>(arg.getAsType())) {
                    const clang::TemplateTypeParmDecl* param = type->getDecl();
                    if(param && param->hasDefaultArgument()) {
                        result.push_back(param->getDefaultArgument().getArgument());
                        continue;
                    }
                }
            }
            result.push_back(arg);
        }
        return result;
    }

    clang::QualType dealias(clang::QualType type) {
        if(auto DNT = type->getAs<clang::TemplateSpecializationType>()) {
            return clang::QualType(DNT, 0);
        } else if(auto DTST = type->getAs<clang::DependentTemplateSpecializationType>()) {
            auto NNS = clang::NestedNameSpecifier::Create(
                context,
                nullptr,
                false,
                dealias(clang::QualType(DTST->getQualifier()->getAsType(), 0)).getTypePtr());
            return context.getDependentTemplateSpecializationType(DTST->getKeyword(),
                                                                  NNS,
                                                                  DTST->getIdentifier(),
                                                                  resolve(DTST->template_arguments()));
        } else {
            return type;
        }
    }

    clang::QualType resolve(clang::QualType type) {
        while(true) {
            // llvm::outs() <<
            // "--------------------------------------------------------------------\n";
            // type.dump();

            clang::MultiLevelTemplateArgumentList list;
            if(auto DNT = type->getAs<clang::DependentNameType>()) {
                type = resolve(resolve(DNT->getQualifier(), DNT->getIdentifier()));
                for(auto begin = arguments.rbegin(), end = arguments.rend(); begin != end; ++begin) {
                    list.addOuterTemplateArguments((*begin)->first, (*begin)->second, true);
                }
                type = sema.SubstType(dealias(type), list, {}, {});
                arguments.clear();

            } else if(auto DTST = type->getAs<clang::DependentTemplateSpecializationType>()) {
                auto ND = resolve(DTST->getQualifier(), DTST->getIdentifier());
                if(auto TATD = llvm::dyn_cast<clang::TypeAliasTemplateDecl>(ND)) {
                    auto args = resolve(DTST->template_arguments());

                    clang::Sema::CodeSynthesisContext context;
                    context.Entity = TATD;
                    context.Kind = clang::Sema::CodeSynthesisContext::TypeAliasTemplateInstantiation;
                    context.TemplateArgs = args.data();
                    sema.pushCodeSynthesisContext(context);

                    list.addOuterTemplateArguments(TATD, args, true);
                    for(auto begin = arguments.rbegin(), end = arguments.rend(); begin != end; ++begin) {
                        list.addOuterTemplateArguments((*begin)->first, (*begin)->second, true);
                    }

                    // llvm::outs() << "before:
                    // ----------------------------------------------------------------\n";
                    // TATD->getTemplatedDecl()->getUnderlyingType().dump();
                    type = dealias(TATD->getTemplatedDecl()->getUnderlyingType());
                    // llvm::outs() << "arguments:
                    // -------------------------------------------------------------\n";
                    // list.dump();
                    type = sema.SubstType(type, list, {}, {});
                    // type.dump();
                    arguments.clear();

                } else {
                    ND->dump();
                    std::terminate();
                }
                // return resolve(DTST);
            } else if(auto LRT = type->getAs<clang::LValueReferenceType>()) {
                type = context.getLValueReferenceType(resolve(LRT->getPointeeType()));
            } else {
                return type;
            }
        }
    }

    clang::QualType resolve(clang::NamedDecl* ND) {
        if(auto TD = llvm::dyn_cast<clang::TypedefDecl>(ND)) {
            return TD->getUnderlyingType();
        } else if(auto TAD = llvm::dyn_cast<clang::TypeAliasDecl>(ND)) {
            return TAD->getUnderlyingType();
        } else {
            ND->dump();
            std::terminate();
        }
    }

    clang::NamedDecl* resolve(const clang::NestedNameSpecifier* NNS, const clang::IdentifierInfo* II) {
        switch(NNS->getKind()) {
            // prefix is an identifier, e.g. <...>::name::
            case clang::NestedNameSpecifier::SpecifierKind::Identifier: {
                return lookup(resolve(resolve(NNS->getPrefix(), NNS->getAsIdentifier())), II);
            }

            // prefix is a type, e.g. <...>::typename name::
            case clang::NestedNameSpecifier::SpecifierKind::TypeSpec:
            case clang::NestedNameSpecifier::SpecifierKind::TypeSpecWithTemplate: {
                return lookup(clang::QualType(NNS->getAsType(), 0), II);
            }

            default: {
                NNS->dump();
                std::terminate();
            }
        }
    }

    clang::NamedDecl* lookup(clang::QualType Type, const clang::IdentifierInfo* Name) {
        clang::NamedDecl* TemplateDecl;
        clang::ArrayRef<clang::TemplateArgument> arguments;

        // llvm::outs() << "--------------------------------------------------------------------\n";

        if(auto TTPT = Type->getAs<clang::TemplateTypeParmType>()) {
            Type->dump();
            std::terminate();
        } else if(auto TST = Type->getAs<clang::TemplateSpecializationType>()) {
            auto TemplateName = TST->getTemplateName();
            TemplateDecl = TemplateName.getAsTemplateDecl();
            arguments = TST->template_arguments();
        } else if(auto DTST = Type->getAs<clang::DependentTemplateSpecializationType>()) {
            TemplateDecl = resolve(DTST->getQualifier(), DTST->getIdentifier());
            arguments = DTST->template_arguments();
        } else if(auto RT = Type->getAs<clang::RecordType>()) {
            return RT->getDecl()->lookup(Name).front();
        } else {
            Type->dump();
            std::terminate();
        }

        clang::NamedDecl* result;
        clang::Sema::CodeSynthesisContext context;

        if(auto CTD = llvm::dyn_cast<clang::ClassTemplateDecl>(TemplateDecl)) {
            // always we check main template first
            result = CTD->getTemplatedDecl()->lookup(Name).front();
            if(result) {
                this->arguments.push_back(new std::pair<clang::Decl*, std::vector<clang::TemplateArgument>>{
                    TemplateDecl,
                    resolve(arguments),
                });
                context.Entity = TemplateDecl;
                context.Kind = clang::Sema::CodeSynthesisContext::TemplateInstantiation;
                context.TemplateArgs = this->arguments.back()->second.data();
                sema.pushCodeSynthesisContext(context);
            } else {
                llvm::SmallVector<clang::ClassTemplatePartialSpecializationDecl*> paritals;
                CTD->getPartialSpecializations(paritals);

                clang::sema::TemplateDeductionInfo info(CTD->getLocation());
                for(auto partial: paritals) {
                    auto deduction = sema.DeduceTemplateArguments(partial, arguments, info);
                    if(deduction == clang::TemplateDeductionResult::Success) {
                        auto decl = partial->lookup(Name).front();
                        if(decl) {
                            result = decl;

                            // TODO: fix resugar
                            // std::vector<clang::TemplateArgument> arguments;
                            auto arguments = info.takeSugared();
                            for(auto& arg: arguments->asArray()) {
                                arg.dump();
                                // if(arg.getKind() == clang::TemplateArgument::ArgKind::Type) {
                                //     auto type = arg.getAsType();
                                //     type->dump();
                                //     if(type.isCanonical()) {
                                //         auto TemplateDecl =
                                //             llvm::dyn_cast<clang::TemplateDecl>(this->arguments.back()->first);
                                //         auto params = TemplateDecl->getTemplateParameters();
                                //         auto TTPT = type->getAs<clang::TemplateTypeParmType>();
                                //         unsigned Depth = TTPT->getDepth();
                                //         unsigned Index = TTPT->getIndex();
                                //         auto* TTPD = dyn_cast<clang::TemplateTypeParmDecl>(params->getParam(Index));
                                //         arguments.push_back(
                                //             clang::TemplateArgument(this->context.getTypeDeclType(TTPD)));
                                //     } else {
                                //         arguments.push_back(arg);
                                //     }
                                // } else {
                                //     arguments.push_back(arg);
                                // }
                            }

                            this->arguments.push_back(new std::pair<clang::Decl*, std::vector<clang::TemplateArgument>>{
                                partial,
                                arguments->asArray(),
                            });
                            context.Entity = partial;
                            context.Kind = clang::Sema::CodeSynthesisContext::TemplateInstantiation;
                            context.TemplateArgs = this->arguments.back()->second.data();
                            sema.pushCodeSynthesisContext(context);
                            break;
                        }
                    }
                }
            }

        } else if(auto TATD = llvm::dyn_cast<clang::TypeAliasTemplateDecl>(TemplateDecl)) {
            result = lookup(TATD->getTemplatedDecl()->getUnderlyingType(), Name);
        }

        if(result == nullptr) {
            Type.dump();
            std::terminate();
        }

        return result;
    }

private:
    clang::Sema& sema;
    clang::ASTContext& context;
    std::vector<std::pair<clang::Decl*, std::vector<clang::TemplateArgument>>*> arguments;
};

}  // namespace clice
