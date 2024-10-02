#include <Compiler/Resolver.h>
#include <clang/Sema/TreeTransform.h>

namespace clice {

namespace {

class TemplateResolverImpl : public clang::TreeTransform<TemplateResolverImpl> {
private:
    struct Frame {
        clang::NamedDecl* decl;
        llvm::ArrayRef<clang::TemplateArgument> arguments;
    };

    using Names = llvm::SmallVector<clang::NamedDecl*, 4>;
    using Base = clang::TreeTransform<TemplateResolverImpl>;

public:
    TemplateResolverImpl(clang::Sema& sema) : TreeTransform(sema), sema(sema), context(sema.getASTContext()) {}

    /// Because Sema::SubsType does not handle alias types, meaning that if the aliased
    /// type contains template parameters, it will not substitute them either, so we need
    /// to remove the alias ourselves.
    clang::QualType TransformTypedefType(clang::TypeLocBuilder& TLB, clang::TypedefTypeLoc TL) {
        if(clang::TypedefNameDecl* TND = TL.getTypedefNameDecl()) {
            auto TSI = TransformType(TND->getTypeSourceInfo());
            TLB.pushTrivial(context, TSI->getType(), {});
            return TSI->getType();
        }

        return clang::QualType();
    }

    /// Sometimes the outer argument is just a simple type `T` and actually cannot make
    /// instantiation continue. In this case, we try to use its default to replace it, which
    /// may make the instantiation continue.
    clang::QualType TransformTemplateTypeParmType(clang::TypeLocBuilder& TLB,
                                                  clang::TemplateTypeParmTypeLoc TL,
                                                  bool SuppressObjCLifetime = false) {
        if(clang::TemplateTypeParmDecl* TTPD = TL.getDecl()) {
            if(TTPD->hasDefaultArgument()) {
                const clang::TemplateArgument& argument = TTPD->getDefaultArgument().getArgument();
                clang::QualType type = TransformType(argument.getAsType());
                TLB.pushTrivial(context, type, {});
                return type;
            }
        }

        TLB.push<clang::TemplateTypeParmTypeLoc>(TL.getType());
        return TL.getType();
    }

    /// `TransformDependentNameType` is the most important function in `TemplateResolver`.
    /// Everytime we meet a `DependentNameType`, we try to resolve it to a simpler type.
    clang::QualType TransformDependentNameType(clang::TypeLocBuilder& TLB,
                                               clang::DependentNameTypeLoc TL,
                                               bool DeducedTSTContext = false) {
        auto NNS = TransformNestedNameSpecifierLoc(TL.getQualifierLoc()).getNestedNameSpecifier();
        auto II = TL.getTypePtr()->getIdentifier();

        Names names;
        if(lookup(names, NNS, II) && names.size() == 1) {
            clang::QualType type = TransformType(substitute(names.front()));
            TLB.pushTrivial(context, type, {});
            return type;
        }
        return clang::QualType();
    }

    // clang::QualType TransformDependentTemplateSpecializationType(clang::TypeLocBuilder& TLB,
    //                                                              clang::DependentTemplateSpecializationTypeLoc TL,
    //                                                              bool DeducedTSTContext = false) {
    //     // Base::TransformDependentTemplateSpecializationType()
    //     // DependentNameResolver resolver(sema, context);
    //     // auto result = resolver.resolve(TL.getType());
    //     // TLB.pushTrivial(context, result, {});
    //     // TODO:
    // }

    bool lookup(Names& names, clang::QualType type, const clang::IdentifierInfo* II) {
        type = TransformType(type);

        llvm::outs() << "lookup {" + II->getName() + "} in {" + type.getAsString() + "}\n";

        clang::TemplateDecl* TD;
        llvm::ArrayRef<clang::TemplateArgument> args;

        if(auto TST = type->getAs<clang::TemplateSpecializationType>()) {
            TD = TST->getTemplateName().getAsTemplateDecl();
            args = TST->template_arguments();
        } else if(auto DTST = type->getAs<clang::DependentTemplateSpecializationType>()) {
            Names members;
            if(lookup(members, DTST->getQualifier(), DTST->getIdentifier()) && members.size() == 1) {
                TD = llvm::dyn_cast<clang::TemplateDecl>(members.front());
                args = DTST->template_arguments();
            } else {
                return false;
            }
        }

        if(auto CTD = llvm::dyn_cast<clang::ClassTemplateDecl>(TD)) {
            return lookup(names, CTD, II, args);
        } else if(auto TATD = llvm::dyn_cast<clang::TypeAliasTemplateDecl>(TD)) {
            frames.emplace_back(TATD, args);
            return lookup(names, substitute(TATD->getTemplatedDecl()->getUnderlyingType()), II);
        }

        return true;
    }

    bool lookup(Names& names, const clang::NestedNameSpecifier* NNS, const clang::IdentifierInfo* II) {
        switch(NNS->getKind()) {
            case clang::NestedNameSpecifier::Identifier: {
                Names members;
                if(lookup(members, NNS->getPrefix(), NNS->getAsIdentifier()) && members.size() == 1) {
                    return lookup(names, substitute(members.front()), II);
                }
            }

            case clang::NestedNameSpecifier::TypeSpec:
            case clang::NestedNameSpecifier::TypeSpecWithTemplate: {
                return lookup(names, clang::QualType(NNS->getAsType(), 0), II);
            }

            default: {
                break;
            }
        }
        return false;
    }

    bool lookup(Names& names,
                clang::ClassTemplateDecl* CTD,
                const clang::IdentifierInfo* II,
                llvm::ArrayRef<clang::TemplateArgument> arguments) {
        // main template first
        auto decls = CTD->getTemplatedDecl()->lookup(II);
        if(!decls.empty()) {
            std::size_t count = 0;
            for(auto decl: decls) {
                names.push_back(decl);
            }

            frames.emplace_back(CTD, arguments);
            return true;
        }

        for(auto base: CTD->getTemplatedDecl()->bases()) {
            // store the current state
            auto copy = frames;

            // try to find the member in the base class
            frames.emplace_back(CTD, arguments);
            if(lookup(names, substitute(base.getType()), II)) {
                return true;
            }

            // if failed, restore the state
            frames = std::move(copy);
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
                names.push_back(decl);
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

    clang::QualType substitute(clang::QualType type) {
        type = TransformType(type);

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
        llvm::outs() << "substitute {" + type.getAsString() + "} to {" + result.getAsString() + "}\n";
        frames.clear();
        return result;
    }

    clang::QualType substitute(clang::NamedDecl* decl) {
        if(auto TND = llvm::dyn_cast<clang::TypedefNameDecl>(decl)) {
            return substitute(TND->getUnderlyingType());
        }

        return clang::QualType();
    }

private:
    clang::Sema& sema;
    clang::ASTContext& context;
    std::vector<Frame> frames;
};

}  // namespace

clang::QualType TemplateResolver::resolve(clang::QualType type) {
    TemplateResolverImpl resolver(sema);
    return resolver.TransformType(type);
}

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

    if(auto TST = type->getAs<clang::TemplateSpecializationType>()) {
        std::vector<clang::TemplateArgument> args;
        for(auto arg: TST->template_arguments()) {
            if(arg.getKind() == clang::TemplateArgument::ArgKind::Type) {
                args.push_back(resolve(arg.getAsType()));
            } else {
                args.push_back(arg);
            }
        }
        return context.getTemplateSpecializationType(TST->getTemplateName(), args);
    } else if(auto DNT = llvm::dyn_cast<clang::DependentNameType>(type)) {
        return resolve(DNT);
    } else if(auto DTST = llvm::dyn_cast<clang::DependentTemplateSpecializationType>(type)) {
        return resolve(DTST);
    } else if(auto LVRT = llvm::dyn_cast<clang::LValueReferenceType>(type)) {
        return context.getLValueReferenceType(resolve(LVRT->getPointeeType()));
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
            frames.emplace_back(TATD, resugar(DTST->template_arguments()));
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
        case clang::NestedNameSpecifier::SpecifierKind::TypeSpec: {
            return lookup(result, clang::QualType(NNS->getAsType(), 0), II);
        }
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
    if(auto DNT = type->getAs<clang::DependentNameType>()) {
        return lookup(result, resolve(DNT), II);
    }

    clang::TemplateDecl* TD;
    std::vector<clang::TemplateArgument> args;

    // FIXME: consider default arguments
    if(auto TST = type->getAs<clang::TemplateSpecializationType>()) {
        TD = TST->getTemplateName().getAsTemplateDecl();
        args = resugar(TST->template_arguments());
    } else if(auto DTST = type->getAs<clang::DependentTemplateSpecializationType>()) {
        if(lookup(result, DTST->getQualifier(), DTST->getIdentifier()) && result.size() == 1) {
            TD = llvm::dyn_cast<clang::TemplateDecl>(result.front());
            args = resugar(DTST->template_arguments());
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

    type->dump();
    llvm::outs() << II->getName() << "\n";
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

    for(auto base: CTD->getTemplatedDecl()->bases()) {
        // store the current state
        auto copy = frames;

        // try to find the member in the base class
        frames.emplace_back(CTD, arguments);
        if(lookup(result, substitute(base.getType()), II)) {
            return true;
        }

        // if failed, restore the state
        frames = std::move(copy);
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

/// FIXME: handle more cese
std::vector<clang::TemplateArgument> DependentNameResolver::resugar(llvm::ArrayRef<clang::TemplateArgument> arguments) {
    std::vector<clang::TemplateArgument> result;
    for(auto arg: arguments) {
        if(arg.getKind() == clang::TemplateArgument::ArgKind::Type) {
            // check whether it is a TemplateTypeParmType.
            if(auto type = llvm::dyn_cast<clang::TemplateTypeParmType>(arg.getAsType())) {
                const clang::TemplateTypeParmDecl* param = type->getDecl();
                if(param && param->hasDefaultArgument()) {
                    result.push_back(param->getDefaultArgument().getArgument());
                } else {
                    result.emplace_back(resolve(arg.getAsType()));
                }
                continue;
            }
        }
        result.push_back(arg);
    }
    return result;
}

// FIXME: handle more case
static bool isalias(clang::QualType type) {
    if(!type->isDependentType()) {
        return false;
    }

    if(auto TAT = type->getAs<clang::TypedefType>()) {
        return true;
    } else if(auto DNT = type->getAs<clang::TemplateSpecializationType>()) {
        for(auto arg: DNT->template_arguments()) {
            if(arg.getKind() == clang::TemplateArgument::ArgKind::Type) {
                if(isalias(arg.getAsType())) {
                    return true;
                }
            }
        }
        return false;
    } else if(auto DNT = type->getAs<clang::DependentNameType>()) {
        return isalias(clang::QualType(DNT->getQualifier()->getAsType(), 0));
    } else if(auto DTST = type->getAs<clang::DependentTemplateSpecializationType>()) {
        return isalias(clang::QualType(DTST->getQualifier()->getAsType(), 0));
    } else if(auto LVRT = type->getAs<clang::LValueReferenceType>()) {
        return isalias(LVRT->getPointeeType());
    } else if(auto TTPT = type->getAs<clang::TemplateTypeParmType>()) {
        return false;
    } else {
        return false;
    }
}

clang::QualType DependentNameResolver::dealias(clang::QualType type) {
    if(!isalias(type)) {
        return type;
    }

    if(auto TAT = type->getAs<clang::TypedefType>()) {
        return dealias(TAT->getDecl()->getUnderlyingType());
    } else if(auto DNT = type->getAs<clang::TemplateSpecializationType>()) {
        llvm::SmallVector<clang::TemplateArgument> args;
        for(auto arg: DNT->template_arguments()) {
            if(arg.getKind() == clang::TemplateArgument::ArgKind::Type) {
                args.push_back(dealias(arg.getAsType()));
            } else {
                args.push_back(arg);
            }
        }
        return context.getTemplateSpecializationType(DNT->getTemplateName(), args);
    }

    else if(auto DNT = type->getAs<clang::DependentNameType>()) {
        auto type = dealias(clang::QualType(DNT->getQualifier()->getAsType(), 0));
        auto prefix = clang::NestedNameSpecifier::Create(context, nullptr, false, type.getTypePtr());
        return context.getDependentNameType(DNT->getKeyword(), prefix, DNT->getIdentifier());
    } else if(auto DTST = type->getAs<clang::DependentTemplateSpecializationType>()) {
        auto type = dealias(clang::QualType(DTST->getQualifier()->getAsType(), 0));
        auto NNS = clang::NestedNameSpecifier::Create(context, nullptr, false, type.getTypePtr());
        auto keyword = DTST->getKeyword();
        auto identifier = DTST->getIdentifier();
        return context.getDependentTemplateSpecializationType(keyword, NNS, identifier, DTST->template_arguments());
    } else if(auto LVRT = type->getAs<clang::LValueReferenceType>()) {
        return context.getLValueReferenceType(dealias(LVRT->getPointeeType()));
    } else {
        return type;
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

    // type->dump();
    auto result = sema.SubstType(dealias(type), list, {}, {});
    // result->dump();
    // llvm::outs() << "\n--------------------------------------------------\n";
    frames.clear();

    return result;
}

}  // namespace clice
