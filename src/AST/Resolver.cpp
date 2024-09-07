#include "AST/Resolver.h"

namespace clice {

clang::QualType DependentNameResolver::resolve(clang::NamedDecl* ND) {
    if(auto TAD = llvm::dyn_cast<clang::TypeAliasDecl>(ND)) {
        return resolve(substitute(TAD->getUnderlyingType()));
    } else if(auto TND = llvm::dyn_cast<clang::TypedefNameDecl>(ND)) {
        return resolve(substitute(TND->getUnderlyingType()));
    } else {
        std::terminate();
    }
}

clang::QualType DependentNameResolver::resolve(clang::QualType type) {
    if(!type->isDependentType()) {
        return type;
    }

    type = type.getDesugaredType(context);

    if(auto DNT = llvm::dyn_cast<clang::DependentNameType>(type)) {
        return resolve(DNT);
    } else if(auto DTST = llvm::dyn_cast<clang::DependentTemplateSpecializationType>(type)) {
        return resolve(DTST);
    } else {
        return type;
    }
}

clang::QualType DependentNameResolver::resolve(const clang::DependentNameType* DNT) {
    llvm::SmallVector<clang::NamedDecl*> result;
    if(lookup(result, DNT->getQualifier(), DNT->getIdentifier()) && result.size() == 1) {
        return resolve(result.front());
    } else {
        DNT->dump();
        std::terminate();
    }
}

clang::QualType DependentNameResolver::resolve(const clang::DependentTemplateSpecializationType* DTST) {
    llvm::SmallVector<clang::NamedDecl*> result;
    if(lookup(result, DTST->getQualifier(), DTST->getIdentifier()) && result.size() == 1) {
        if(auto TATD = llvm::dyn_cast<clang::TypeAliasTemplateDecl>(result.front())) {
            frames.emplace_back(TATD, DTST->template_arguments());
            return resolve(substitute(TATD->getTemplatedDecl()->getUnderlyingType()));
        }
    }

    DTST->dump();
    std::terminate();
}

bool DependentNameResolver::lookup(llvm::SmallVector<clang::NamedDecl*>& result,
                                   const clang::NestedNameSpecifier* NNS,
                                   const clang::IdentifierInfo* II) {
    switch(NNS->getKind()) {
        // prefix is an identifier, e.g. <...>::name::
        case clang::NestedNameSpecifier::SpecifierKind::Identifier: {
            if(lookup(result, NNS->getPrefix(), NNS->getAsIdentifier()) && result.size() == 1) {
                auto type = resolve(result.front());
                result.clear();
                return lookup(result, type, II);
            } else {
                NNS->dump();
                std::terminate();
            }
        }

        // prefix is a type, e.g. <...>::typename name::
        case clang::NestedNameSpecifier::SpecifierKind::TypeSpec:
        case clang::NestedNameSpecifier::SpecifierKind::TypeSpecWithTemplate: {
            return lookup(result, clang::QualType(NNS->getAsType(), 0), II);
        }

        default: {
            NNS->dump();
            std::terminate();
        }
    }
}

bool DependentNameResolver::lookup(llvm::SmallVector<clang::NamedDecl*>& result,
                                   const clang::QualType type,
                                   const clang::IdentifierInfo* II) {

    clang::TemplateDecl* TD;
    llvm::ArrayRef<clang::TemplateArgument> args;

    // FIXME: consider default arguments
    if(auto TST = type->getAs<clang::TemplateSpecializationType>()) {
        TD = TST->getTemplateName().getAsTemplateDecl();
        args = TST->template_arguments();
    } else if(auto DTST = type->getAs<clang::DependentTemplateSpecializationType>()) {
        if(lookup(result, DTST->getQualifier(), DTST->getIdentifier()) && result.size() == 1) {
            TD = llvm::dyn_cast<clang::TemplateDecl>(result.front());
            args = DTST->template_arguments();
            result.clear();
        } else {
            return false;
        }
    }

    if(auto CTD = llvm::dyn_cast<clang::ClassTemplateDecl>(TD)) {
        return lookup(result, CTD, II, args);
    } else if(auto TATD = llvm::dyn_cast<clang::TypeAliasTemplateDecl>(TD)) {
        frames.emplace_back(TATD, args);
        return lookup(result, resolve(substitute(TATD->getTemplatedDecl()->getUnderlyingType())), II);
    }

    std::terminate();
}

bool DependentNameResolver::lookup(llvm::SmallVector<clang::NamedDecl*>& result,
                                   clang::ClassTemplateDecl* CTD,
                                   const clang::IdentifierInfo* II,
                                   llvm::ArrayRef<clang::TemplateArgument> arguments) {

    // main template first
    auto decls = CTD->getTemplatedDecl()->lookup(II);
    if(!decls.empty()) {
        std::size_t count = 0;
        for(auto decl: decls) {
            result.push_back(decl);
        }

        frames.emplace_back(CTD, arguments);
        return true;
    }

    // if failed, try partial specializations
    llvm::SmallVector<clang::ClassTemplatePartialSpecializationDecl*> partials;
    CTD->getPartialSpecializations(partials);

    for(auto partial: partials) {
        clang::sema::TemplateDeductionInfo info(partial->getLocation());
        if(sema.DeduceTemplateArguments(partial, arguments, info) != clang::TemplateDeductionResult::Success) {
            break;
        }

        auto decls = partial->lookup(II);
        if(decls.empty()) {
            break;
        }

        for(auto decl: decls) {
            result.push_back(decl);
        }

        // NOTE: takeSugared will take the ownership of the list
        auto list = info.takeSugared();
        frames.emplace_back(partial, list->asArray());
        // FIXME: should we delete the list?
        // delete list;
        return true;
    }

    return false;
}

// FIXME: handle more case
static bool isalias(clang::QualType type) {
    if(!type->isDependentType()) {
        return false;
    }

    if(auto TAT = type->getAs<clang::TypedefType>()) {
        return true;
    } else if(auto DNT = type->getAs<clang::TemplateSpecializationType>()) {
        return false;
    } else if(auto DNT = type->getAs<clang::DependentNameType>()) {
        return isalias(clang::QualType(DNT->getQualifier()->getAsType(), 0));
    } else if(auto LVRT = type->getAs<clang::LValueReferenceType>()) {
        return isalias(LVRT->getPointeeType());
    } else {
        type.dump();
        std::terminate();
    }
}

clang::QualType DependentNameResolver::dealias(clang::QualType type) {
    if(!isalias(type)) {
        return type;
    }

    if(auto TAT = type->getAs<clang::TypedefType>()) {
        return dealias(TAT->getDecl()->getUnderlyingType());
    } else if(auto DNT = type->getAs<clang::DependentNameType>()) {
        auto type = dealias(clang::QualType(DNT->getQualifier()->getAsType(), 0));
        auto prefix = clang::NestedNameSpecifier::Create(context, nullptr, false, type.getTypePtr());
        return context.getDependentNameType(DNT->getKeyword(), prefix, DNT->getIdentifier());
    } else if(auto LVRT = type->getAs<clang::LValueReferenceType>()) {
        return context.getLValueReferenceType(dealias(LVRT->getPointeeType()));
    } else {
        std::terminate();
    }
}

clang::QualType DependentNameResolver::substitute(clang::QualType type) {
    clang::MultiLevelTemplateArgumentList list;
    for(auto begin = frames.rbegin(), end = frames.rend(); begin != end; ++begin) {
        list.addOuterTemplateArguments(begin->decl, begin->arguments, true);
    }

    for(auto frame: frames) {
        clang::Sema::CodeSynthesisContext context;
        context.Entity = frame.decl;
        context.TemplateArgs = frame.arguments.data();
        context.Kind = clang::Sema::CodeSynthesisContext::TemplateInstantiation;
        sema.pushCodeSynthesisContext(context);
    }

    auto result = sema.SubstType(dealias(type), list, {}, {});
    frames.clear();

    return result;
}

}  // namespace clice
