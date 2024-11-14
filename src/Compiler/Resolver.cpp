#include <Compiler/Resolver.h>
#include <clang/Sema/Template.h>
#include <clang/Sema/TreeTransform.h>
#include <clang/Sema/TemplateDeduction.h>
#include <ranges>

namespace clice {

namespace {

/// `Sema::SubstType` will not substitute template arguments in aliased types.
/// For example:
///
/// ```cpp
/// template <typename T>
/// struct A {
///     using base = std::vector<T>;
///     using type = typename base::reference;
/// };
/// ```
///
/// In this case, if you call `SubstType` on `type`, the alias `base` will remain with
/// the original type parameter `T`, without substituting it. Therefore, we need to
/// manually resolve the alias before calling `SubstType`, which is what `DealiasOnly`
/// aims to achieve.
class DealiasOnly : public clang::TreeTransform<DealiasOnly> {
public:
    DealiasOnly(clang::Sema& sema) : TreeTransform(sema), context(sema.getASTContext()) {}

    // FIXME: desugar more types, e.g `UsingType`.

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

    clang::QualType TransformElaboratedType(clang::TypeLocBuilder& TLB,
                                            clang::ElaboratedTypeLoc TL) {
        clang::QualType type = TransformType(TL.getNamedTypeLoc().getType());
        TLB.pushTrivial(context, type, {});
        return type;
    }

private:
    clang::ASTContext& context;
};

/// A helper class to record the instantiation stack.
struct InstantiationStack {
    using Arguments = llvm::SmallVector<clang::TemplateArgument, 4>;
    using TemplateArguments = llvm::ArrayRef<clang::TemplateArgument>;

    llvm::SmallVector<std::pair<clang::Decl*, Arguments>> data;

    void clear() {
        data.clear();
    }

    auto state() const {
        return data;
    }

    void rewind(auto& point) {
        data = std::move(point);
    }

    void push(clang::Decl* decl, TemplateArguments arguments) {
        data.emplace_back(decl, arguments);
    }

    auto& frames() {
        return data;
    }
};

/// The core class that performs pseudo template instantiation.
class PseudoInstantiator : public clang::TreeTransform<PseudoInstantiator> {
public:
    using Base = clang::TreeTransform<PseudoInstantiator>;

    using TemplateArguments = llvm::ArrayRef<clang::TemplateArgument>;

    using TemplateDeductionInfo = clang::sema::TemplateDeductionInfo;

    PseudoInstantiator(clang::Sema& sema, llvm::DenseMap<const void*, clang::QualType>& resolved) :
        Base(sema), sema(sema), context(sema.getASTContext()), resolved(resolved) {}

public:
    /// If this class and its base class have members with the same name, `DeclContext::lookup`
    /// will return multiple declarations in order from the base class to the derived class, so we
    /// use the last declaration.
    clang::Decl* preferred(clang::lookup_result members) {
        clang::Decl* decl = nullptr;
        std::ranges::for_each(members, [&](auto member) { decl = member; });
        return decl;
    }

    clang::lookup_result lookup(clang::QualType type, clang::DeclarationName name) {
        clang::Decl* TD = nullptr;
        llvm::ArrayRef<clang::TemplateArgument> args;
        type = TransformType(type);

        if(auto TST = type->getAs<clang::TemplateSpecializationType>()) {
            TD = TST->getTemplateName().getAsTemplateDecl();
            args = TST->template_arguments();
        } else if(auto DTST = type->getAs<clang::DependentTemplateSpecializationType>()) {
            if(auto decl = preferred(lookup(DTST->getQualifier(), DTST->getIdentifier()))) {
                TD = decl;
                args = DTST->template_arguments();
            }
        }

        if(!TD) {
            return clang::lookup_result();
        }

        if(auto CTD = llvm::dyn_cast<clang::ClassTemplateDecl>(TD)) {
            return lookup(CTD, name, args);
        } else if(auto TATD = llvm::dyn_cast<clang::TypeAliasTemplateDecl>(TD)) {
            stack.push(TATD, args);
            return lookup(instantiate(TATD->getTemplatedDecl()->getUnderlyingType()), name);
        }

        return clang::lookup_result();
    }

    /// Look up the name in the given nested name specifier.
    clang::lookup_result lookup(const clang::NestedNameSpecifier* NNS,
                                clang::DeclarationName name) {
        /// Search the resolved entities first.
        if(auto iter = resolved.find(NNS); iter != resolved.end()) {
            return lookup(iter->second, name);
        }

        switch(NNS->getKind()) {
            case clang::NestedNameSpecifier::Identifier: {
                /// If the prefix is `Identifier`, it must be a dependent name.
                /// For example: `std::vector<T>::value_type::type`
                ///               ^~~~~~~~~~~~~~~~~~~~~~~~~^
                ///                        identifier
                ///
                /// So resolve it recursively.
                auto type =
                    instantiate(preferred(lookup(NNS->getPrefix(), NNS->getAsIdentifier())));
                resolved.try_emplace(NNS, type);
                return lookup(type, name);
            }

            case clang::NestedNameSpecifier::TypeSpec:
            case clang::NestedNameSpecifier::TypeSpecWithTemplate: {
                /// If the prefix is `TypeSpec` or `TypeSpecWithTemplate`, it must be a type.
                return lookup(clang::QualType(NNS->getAsType(), 0), name);
            }

            case clang::NestedNameSpecifier::Global:
            case clang::NestedNameSpecifier::Namespace:
            case clang::NestedNameSpecifier::NamespaceAlias:
            case clang::NestedNameSpecifier::Super: {
                llvm::errs() << "Unexpected name specifier\n";
                std::terminate();
            }
        }
    }

    /// Look up the name in the given class template. We first search the name in the
    /// primary template, if failed, try dependent base classes, if still failed, try
    /// partial specializations. **Note that this function will be responsible for pushing
    /// the class template and its template arguments to the instantiation stack**.
    clang::lookup_result lookup(clang::ClassTemplateDecl* CTD,
                                clang::DeclarationName name,
                                TemplateArguments arguments) {
        /// First, try to find the name in the primary template.
        if(auto members = CTD->getTemplatedDecl()->lookup(name); !members.empty()) {
            clang::TemplateParameterList* list = CTD->getTemplateParameters();
            TemplateArguments params = list->getInjectedTemplateArgs(context);
            TemplateDeductionInfo info = {clang::SourceLocation(), list->getDepth()};
            llvm::SmallVector<clang::DeducedTemplateArgument, 4> deduced(arguments.size());
            auto result =
                sema.DeduceTemplateArguments(list, params, arguments, info, deduced, false);
            if(result == clang::TemplateDeductionResult::Success) {
                llvm::SmallVector<clang::TemplateArgument, 4> list(deduced.begin(), deduced.end());
                stack.push(CTD, list);
                return members;
            }
        }

        /// Try to find the member in the base class.
        for(auto base: CTD->getTemplatedDecl()->bases()) {
            if(auto type = base.getType(); type->isDependentType()) {
                /// Because we instantiate the base class, this will clear the instantiation stack.
                /// If the lookup fails, we need to rewind the stack to try the next base class.
                auto state = stack.state();
                stack.push(CTD, arguments);

                if(auto members = lookup(instantiate(type), name); !members.empty()) {
                    return members;
                }

                stack.rewind(state);
            }
        }

        /// Try to find the name in the partial specializations.
        llvm::SmallVector<clang::ClassTemplatePartialSpecializationDecl*> partials;
        CTD->getPartialSpecializations(partials);

        for(auto partial: partials) {
            TemplateDeductionInfo info{clang::SourceLocation(),
                                       partial->getTemplateParameters()->getDepth()};
            auto result = sema.DeduceTemplateArguments(partial, arguments, info);
            if(result == clang::TemplateDeductionResult::Success) {
                if(auto members = partial->lookup(name); !members.empty()) {
                    auto list = info.takeSugared();
                    stack.push(partial, list->asArray());
                    // FIXME: should we delete the list?
                    return members;
                }
            }
        }

        /// FIXME: try full specializations?.

        return clang::lookup_result();
    }

    /// Instantiate the given type and clear the instantiation stack.
    clang::QualType instantiate(clang::QualType type) {
        if(!type->isDependentType()) {
            return type;
        }

        auto& contexts = sema.CodeSynthesisContexts;
        assert(contexts.empty() && "CodeSynthesisContexts should be empty");

        std::ranges::for_each(stack.frames(), [&](auto& frame) {
            clang::Sema::CodeSynthesisContext context;
            context.Entity = frame.first;
            context.TemplateArgs = frame.second.data();
            context.Kind = clang::Sema::CodeSynthesisContext::TemplateInstantiation;
            contexts.push_back(context);
        });

        clang::MultiLevelTemplateArgumentList list;
        std::ranges::for_each(stack.frames() | std::ranges::views::reverse, [&](auto& frame) {
            list.addOuterTemplateArguments(frame.first, frame.second, true);
        });

        type = DealiasOnly(sema).TransformType(type);
        auto result = sema.SubstType(type, list, {}, {});

        stack.clear();
        contexts.clear();

        return result;
    }

    clang::QualType instantiate(clang::Decl* decl) {
        if(!decl) {
            return clang::QualType();
        }

        if(auto TND = llvm::dyn_cast<clang::TypedefNameDecl>(decl)) {
            return instantiate(TND->getUnderlyingType());
        } else if(auto RD = llvm::dyn_cast<clang::RecordDecl>(decl)) {
            return clang::QualType(RD->getTypeForDecl(), 0);
        }

        // FIXME: more possibilities?

        return clang::QualType();
    }

public:
    /// Sometimes the outer argument is just a simple type `T` and actually cannot make
    /// instantiation continue. In this case, we try to use its default argument to replace it,
    /// which may make the instantiation continue.
    /// For example: `template <typename T = std::vector<T>> using type = T::value_type`.
    clang::QualType TransformTemplateTypeParmType(clang::TypeLocBuilder& TLB,
                                                  clang::TemplateTypeParmTypeLoc TL,
                                                  bool SuppressObjCLifetime = false) {
        if(clang::TemplateTypeParmDecl* TTPD = TL.getDecl()) {
            if(TTPD->hasDefaultArgument()) {
                const clang::TemplateArgument& argument = TTPD->getDefaultArgument().getArgument();
                clang::QualType type = TransformType(argument.getAsType());
                TLB.pushTrivial(context, type, clang::SourceLocation());
                return type;
            }
        }

        TLB.push<clang::TemplateTypeParmTypeLoc>(TL.getType());
        return TL.getType();
    }

    clang::QualType TransformDependentNameType(clang::TypeLocBuilder& TLB,
                                               clang::DependentNameTypeLoc TL,
                                               bool DeducedTSTContext = false) {
        auto DNT = TL.getTypePtr();

        /// Search the resolved entities first.
        if(auto iter = resolved.find(DNT); iter != resolved.end()) {
            TLB.pushTrivial(context, iter->second, {});
            return iter->second;
        }

        auto NNS = TransformNestedNameSpecifierLoc(TL.getQualifierLoc()).getNestedNameSpecifier();
        auto type = TransformType(instantiate(preferred(lookup(NNS, DNT->getIdentifier()))));
        resolved.try_emplace(DNT, type);
        TLB.pushTrivial(context, type, {});
        return type;
    }

    using Base::TransformDependentTemplateSpecializationType;

    /// For a `DependentTemplateSpecializationType`, the template name can be either an alias
    /// template or a class template. If it is an alias template, we can simplify it directly
    /// by transforming the alias template's underlying type. However, if it is a class
    /// template, we need additional context (e.g., suffix name) to simplify it correctly. In
    /// this case, we defer further transformation to `TransformDependentNameType`, which can
    /// handle this scenario. Thus, if the template is not an alias template, we keep it
    /// unchanged here.
    clang::QualType TransformDependentTemplateSpecializationType(
        clang::TypeLocBuilder& TLB,
        clang::DependentTemplateSpecializationTypeLoc TL) {
        auto DTST = TL.getTypePtr();

        /// Search the resolved entities first.
        if(auto iter = resolved.find(DTST); iter != resolved.end()) {
            TLB.pushTrivial(context, iter->second, {});
            return iter->second;
        }

        auto NNS = TransformNestedNameSpecifierLoc(TL.getQualifierLoc()).getNestedNameSpecifier();
        // FIXME: Transform template arguments.

        /// The `lookup` may change the instantiation stack, save the current state.
        /// FIXME:
        auto state = stack.state();
        if(auto decl = preferred(lookup(NNS, DTST->getIdentifier()))) {
            if(auto TATD = llvm::dyn_cast<clang::TypeAliasTemplateDecl>(decl)) {
                stack.push(TATD, DTST->template_arguments());
                clang::QualType type = TATD->getTemplatedDecl()->getUnderlyingType();
                type = TransformType(instantiate(type));
                resolved.try_emplace(DTST, type);
                TLB.pushTrivial(context, type, {});
                return type;
            }
            stack.rewind(state);
        }

        TLB.push<clang::DependentTemplateSpecializationTypeLoc>(TL.getType());
        return TL.getType();
    }

private:
    clang::Sema& sema;
    clang::ASTContext& context;
    InstantiationStack stack;
    llvm::DenseMap<const void*, clang::QualType>& resolved;
};

}  // namespace

clang::QualType TemplateResolver::resolve(clang::QualType type) {
    PseudoInstantiator instantiator(sema, resolved);
    return instantiator.TransformType(type);
}

clang::lookup_result TemplateResolver::resolve(const clang::DependentScopeDeclRefExpr* expr) {
    PseudoInstantiator instantiator(sema, resolved);
    return instantiator.lookup(expr->getQualifier(), expr->getDeclName());
}

}  // namespace clice
