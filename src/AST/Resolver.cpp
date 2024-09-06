#include "AST/Resolver.h"

namespace clice {

clang::QualType DependentNameResolver::resolve(clang::QualType type) {
    if(auto DNT = clang::dyn_cast<clang::DependentNameType>(type)) {
        return resolve(DNT);
    } else {
        std::terminate();
    }
}

clang::QualType DependentNameResolver::resolve(const clang::DependentNameType* DNT) {
    auto prefix = DNT->getQualifier();
    auto II = DNT->getIdentifier();
    llvm::SmallVector<clang::NamedDecl*> result;
    lookup(result, prefix, II);
    assert(result.size() == 1);

    // TODO: substitute template arguments
    if(auto TAD = clang::dyn_cast<clang::TypeAliasDecl>(result.front())) {
        return substitute(TAD->getUnderlyingType());
    } else {
        std::terminate();
    }
}

bool DependentNameResolver::lookup(llvm::SmallVector<clang::NamedDecl*>& result,
                                   clang::ClassTemplateDecl* CTD,
                                   const clang::IdentifierInfo* II,
                                   llvm::ArrayRef<clang::TemplateArgument> arguments) {

    // main template first
    auto decls = CTD->getTemplatedDecl()->lookup(II);
    if(!decls.empty()) {
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
            return false;
        }

        auto decls = partial->lookup(II);
        if(decls.empty()) {
            return false;
        }

        for(auto decl: decls) {
            result.push_back(decl);
        }

        auto list = info.takeSugared();
        frames.emplace_back(partial, list->asArray());
        delete list;
        return true;
    }
}

bool DependentNameResolver::lookup(llvm::SmallVector<clang::NamedDecl*>& result,
                                   const clang::NestedNameSpecifier* NNS,
                                   const clang::IdentifierInfo* II) {
    switch(NNS->getKind()) {
        // prefix is an identifier, e.g. <...>::name::
        case clang::NestedNameSpecifier::SpecifierKind::Identifier: {
            // return lookup(resolve(resolve(NNS->getPrefix(), NNS->getAsIdentifier())),
            // II);
        }

        // prefix is a type, e.g. <...>::typename name::
        case clang::NestedNameSpecifier::SpecifierKind::TypeSpec:
        case clang::NestedNameSpecifier::SpecifierKind::TypeSpecWithTemplate: {
            auto type = NNS->getAsType();
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

        default: {
            NNS->dump();
            std::terminate();
        }
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

    auto result = sema.SubstType(type, list, {}, {});

    frames.clear();

    return result;
}

}  // namespace clice
