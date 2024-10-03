#include <Compiler/Resolver.h>
#include <clang/Sema/TreeTransform.h>

namespace clice {

namespace {

/// `Sema::SubstType` will not substitute in aliased types, e.g.
/// ```cpp
/// template <typename T>
/// struct A {
///     using base = std::vector<T>;
///     using type = typename base::reference;
/// };
/// ```
/// In this case, if you call `SubstType` on `type`, base will keep the same.
/// So we need to dealias the type first before calling `SubstType`.
class DealiasOnly : public clang::TreeTransform<DealiasOnly> {
public:
    DealiasOnly(clang::Sema& sema) : TreeTransform(sema), context(sema.getASTContext()) {}

    clang::QualType TransformTypedefType(clang::TypeLocBuilder& TLB, clang::TypedefTypeLoc TL) {
        if(clang::TypedefNameDecl* TND = TL.getTypedefNameDecl()) {
            auto type = TransformType(TND->getUnderlyingType());
            if(auto ET = llvm::dyn_cast<clang::ElaboratedType>(type)) {
                type = ET->getNamedType();
            }
            TLB.pushTrivial(context, type, {});
            return type;
        }

        return clang::QualType();
    }

private:
    clang::ASTContext& context;
};

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

    clang::QualType TransformElaboratedType(clang::TypeLocBuilder& TLB, clang::ElaboratedTypeLoc TL) {
        clang::QualType type = TransformType(TL.getNamedTypeLoc().getType());
        TLB.pushTrivial(context, type, {});
        return type;
    }

    clang::QualType TransformTypedefType(clang::TypeLocBuilder& TLB, clang::TypedefTypeLoc TL) {
        if(clang::TypedefNameDecl* TND = TL.getTypedefNameDecl()) {
            auto type = TransformType(TND->getUnderlyingType());
            if(auto ET = llvm::dyn_cast<clang::ElaboratedType>(type)) {
                type = ET->getNamedType();
            }
            TLB.pushTrivial(context, type, {});
            return type;
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

    using Base::TransformDependentTemplateSpecializationType;

    clang::QualType TransformDependentTemplateSpecializationType(clang::TypeLocBuilder& TLB,
                                                                 clang::DependentTemplateSpecializationTypeLoc TL) {
        auto NNS = TransformNestedNameSpecifierLoc(TL.getQualifierLoc()).getNestedNameSpecifier();
        auto II = TL.getTypePtr()->getIdentifier();

        Names names;
        auto copy = frames;
        if(lookup(names, NNS, II) && names.size() == 1) {
            if(auto TATD = llvm::dyn_cast<clang::TypeAliasTemplateDecl>(names.front())) {
                // FIXME: Transform template arguments
                frames.emplace_back(TATD, TL.getTypePtr()->template_arguments());
                clang::QualType type = TransformType(substitute(TATD->getTemplatedDecl()->getUnderlyingType()));
                TLB.pushTrivial(context, type, {});
                return type;
            }
        }

        frames = std::move(copy);
        TLB.push<clang::DependentTemplateSpecializationTypeLoc>(TL.getType());
        return TL.getType();
    }

    /// look up the name in the given type.
    bool lookup(Names& names, clang::QualType type, const clang::IdentifierInfo* II) {
        type = TransformType(type);

        llvm::outs() << "lookup { " + II->getName() + " } in { " + type.getAsString() + " }\n";

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

    /// look up the name in the given nested name specifier.
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

    /// look up the name in the given class template. We first try to find the name in the main
    /// template, if failed, we try to find the name in the base classes, if still failed, we try to find
    /// the name in the partial specializations.
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
        type = DealiasOnly(sema).TransformType(type);

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
        llvm::outs() << "substitute { " + type.getAsString() + " } to { " + result.getAsString() + " }\n";
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

}  // namespace clice
