#include "AST/Resolver.h"

namespace clice {

clang::QualType DependentNameResolver::resolve(clang::NamedDecl* ND) {
    auto decl = substitute(ND);
    if(auto TAD = llvm::dyn_cast<clang::TypeAliasDecl>(decl)) {
        return resolve(TAD->getUnderlyingType());
    } else if(auto TND = llvm::dyn_cast<clang::TypedefNameDecl>(decl)) {
        return resolve(TND->getUnderlyingType());
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
    if(auto TST = type->getAs<clang::TemplateSpecializationType>()) {
        auto TD = TST->getTemplateName().getAsTemplateDecl();
        // FIXME: consider default arguments
        auto args = TST->template_arguments();
        if(auto CTD = llvm::dyn_cast<clang::ClassTemplateDecl>(TD)) {
            return lookup(result, CTD, II, args);
        }
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
        delete list;
        return true;
    }

    return false;
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

    auto result = sema.SubstType(type, list, {}, {});

    frames.clear();

    return result;
}

clang::Decl* DependentNameResolver::substitute(clang::Decl* decl) {
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

    // FIXME: use fake TU
    auto result = sema.SubstDecl(decl, context.getTranslationUnitDecl(), list);

    frames.clear();

    return result;
}

}  // namespace clice
