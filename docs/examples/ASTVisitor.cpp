#include <Clang/Clang.h>
#include <clang/Sema/Lookup.h>
#include <clang/Sema/Template.h>

class DependentShaper {
    clang::ASTContext& context;
    clang::Sema& sema;

public:
    DependentShaper(clang::ASTContext& context, clang::Sema& sema) : context(context), sema(sema) {}

    /// replace template arguments in a dependent type
    /// e.g.
    /// ```cpp
    /// template <typename T1, typename T2>
    /// struct A {
    ///     using reference = T1&;
    /// };
    ///
    /// template <typename U1, typename U2>
    /// struct B {
    ///    using type = A<U1, U2>::reference;
    /// };
    /// ```
    /// we want to simplify `A<U1, U2>::reference` to `U1&`, then we need to call
    /// `replace(<T1&>, <T1 = U1, T2 = U2>)` to get `U1&`.
    clang::QualType replace(clang::QualType input,
                            llvm::ArrayRef<clang::TemplateArgument> originalArguments) {
        assert(type->isDependentType() && "type is not dependent");

        // if the input is still a dependent name type, we need to simplify it recursively
        while(auto type = llvm::dyn_cast<clang::DependentNameType>(input)) {
            input = simplify(type);
        }

        clang::ElaboratedType* elaboratedType = nullptr;
        clang::MultiLevelTemplateArgumentList list;
        list.addOuterTemplateArguments(originalArguments);
        llvm::outs() << "-------------------------------------------------\n";
        clang::Sema::CodeSynthesisContext InstContext;
        // InstContext.Kind = clang::Sema::CodeSynthesisContext::TemplateInstantiation;
        // InstContext.Entity = nullptr;
        // InstContext.TemplateArgs = TemplateArgs;
        //  sema.CodeSynthesisContexts.back().dump();
        auto result = sema.SubstType(input, list, {}, {});

        if(auto type = llvm::dyn_cast<clang::LValueReferenceType>(input)) {
            auto pointee = type->getPointeeType();

            while(auto elaboratedType = llvm::dyn_cast<clang::ElaboratedType>(pointee)) {
                pointee = elaboratedType->desugar();
            }

            // get the position of the dependent type in the template parameter list
            // e.g `T1` in <typename T1, typename T2>, the index is 0
            if(auto paramType = llvm::dyn_cast<clang::TemplateTypeParmType>(pointee)) {
                auto index = paramType->getIndex();
                auto argument = originalArguments[index];

                // create a new type that fit the replacement
                return context.getLValueReferenceType(argument.getAsType());

            } else if(auto defType = llvm::dyn_cast<clang::TypedefType>(pointee)) {
                return replace(defType->getDecl()->getUnderlyingType(), originalArguments);
            } else {
                pointee->dump();
                std::terminate();
            }
        }
        // TODO: handle other kinds

        return input;
    }

    clang::NamedDecl* lookup(const clang::ClassTemplateDecl* classTemplateDecl,
                             const clang::IdentifierInfo* identifier) {
        clang::CXXRecordDecl* recordDecl = classTemplateDecl->getTemplatedDecl();
        auto result = recordDecl->lookup(identifier);
        return result.front();
    }

    /// for a complex dependent type: `X<...>::name::name2::...::nameN`, we can resolve it
    /// recursively. so we only need to handle the `X<...>::name`, whose prefix is a template
    /// specialization type.
    clang::QualType simplify(const clang::TemplateSpecializationType* templateType,
                             const clang::IdentifierInfo* identifier) {
        // X is a class template or a type alias template
        auto templateDecl = templateType->getTemplateName().getAsTemplateDecl();
        if(auto classTemplateDecl = llvm::dyn_cast<clang::ClassTemplateDecl>(templateDecl)) {
            // lookup the identifier in the record decl
            auto namedDecl = lookup(classTemplateDecl, identifier);

            if(auto decl = llvm::dyn_cast<clang::TypeAliasDecl>(namedDecl)) {
                return replace(decl->getUnderlyingType(), templateType->template_arguments());
            } else if(auto decl = llvm::dyn_cast<clang::TypedefDecl>(namedDecl)) {
                return replace(decl->getUnderlyingType(), templateType->template_arguments());
            } else {
                namedDecl->dump();
            }

        } else if(auto aliasTemplateDecl =
                      llvm::dyn_cast<clang::TypeAliasTemplateDecl>(templateDecl)) {
            // TODO:
        } else {
            templateDecl->dump();
        }
    }

    const clang::QualType simplify(const clang::NestedNameSpecifier* specifier,
                                   const clang::IdentifierInfo* identifier) {
        auto kind = specifier->getKind();
        switch(specifier->getKind()) {
            case clang::NestedNameSpecifier::Identifier: {
                const auto prefix = simplify(specifier->getPrefix(), specifier->getAsIdentifier());

                if(auto type = llvm::dyn_cast<clang::TemplateSpecializationType>(prefix)) {
                    return simplify(type, identifier);
                } else {
                    prefix->dump();
                }

                break;
            }

            case clang::NestedNameSpecifier::TypeSpec: {
                auto node = specifier->getAsType();

                if(auto type = llvm::dyn_cast<clang::TemplateTypeParmType>(node)) {
                    // represent a direct dependent name, e.g. typename T::^ name
                    // and can not be further simplified
                    // node->dump();
                    type->dump();
                } else if(auto type = node->getAs<clang::TemplateSpecializationType>()) {
                    // represent a dependent name that is a template specialization
                    // e.g. typename vector<int>::^ name, and can be further simplified
                    return simplify(type, identifier);
                } else {
                    node->dump();
                }

                break;
            }

            case clang::NestedNameSpecifier::TypeSpecWithTemplate: {
                llvm::outs() << "unsupported kind: " << kind << "\n";
                break;
            }

            default: {
                llvm::outs() << "unsupported kind: " << kind << "\n";
            }
        }
    }

    const clang::QualType simplify(const clang::DependentNameType* type) {
        // llvm::outs() << "-----------------------------------------------------------" << "\n";
        // type->dump();
        return simplify(type->getQualifier(), type->getIdentifier());
    }
};

namespace clang {
class DependentNameResolver {
public:
    Sema& S;
    ASTContext& Ctx;
    clang::NamedDecl* CurrentDecl;

public:
    DependentNameResolver(ASTContext& Ctx, Sema& S) : Ctx(Ctx), S(S) {}

    std::vector<TemplateArgument> resolve(llvm::ArrayRef<TemplateArgument> arguments) {
        std::vector<TemplateArgument> result;
        for(auto arg: arguments) {
            if(arg.getKind() == TemplateArgument::ArgKind::Type) {
                if(auto type = llvm::dyn_cast<TemplateTypeParmType>(arg.getAsType())) {
                    const TemplateTypeParmDecl* param = type->getDecl();
                    if(param->hasDefaultArgument()) {
                        result.push_back(param->getDefaultArgument().getArgument());
                        continue;
                    }
                }
            }
            result.push_back(arg);
        }
        return result;
    }

    QualType resolve(QualType T) {
        if(!T->isDependentType()) {
            return T;
        }

        while(true) {
            if(auto DNT = T->getAs<DependentNameType>()) {
                T = resolve(DNT);
            } else if(auto DTST = T->getAs<DependentTemplateSpecializationType>()) {
                T = resolve(DTST);
            } else if(auto LRT = T->getAs<LValueReferenceType>()) {
                return Ctx.getLValueReferenceType(resolve(LRT->getPointeeType()));
            } else {
                return T;
            }
        }
    }

    /// resolve a dependent name type, e.g. `typename std::vector<T>::reference`
    QualType resolve(const DependentNameType* DNT) {
        // e.g. when DNT is `typename std::vector<T>::reference`
        // - qualifier: std::vector<T>
        // - identifier: reference
        return resolve(DNT->getQualifier(), DNT->getIdentifier());
    }

    /// resolve a dependent template specialization type.
    QualType resolve(const DependentTemplateSpecializationType* DTST) {
        // e.g. when DTST is `typename std::allocator_traits<Alloc>::template rebind_alloc<T>`.
        // - qualifier: std::allocator_traits<Alloc>
        // - identifier: rebind_alloc
        // - template_arguments: <T>
        return resolve(DTST->getQualifier(), DTST->getIdentifier(), DTST->template_arguments());
    }

    QualType resolve(const NestedNameSpecifier* TST,
                     const IdentifierInfo* II,
                     ArrayRef<TemplateArgument> arguments = {}) {

        switch(TST->getKind()) {
            case NestedNameSpecifier::SpecifierKind::Identifier: {
                llvm::outs() << "\n------------------ Identifier -----------------------\n";
                // when the kind of TST is Identifier
                // e.g. std::vector<std::vector<T>>::value_type::
                // resolve it recursively
                return resolve(resolve(TST->getPrefix(), TST->getAsIdentifier()), II, arguments);
            }

            case NestedNameSpecifier::SpecifierKind::TypeSpec: {
                llvm::outs() << "\n------------------ TypeSpec -----------------------\n";
                TST->dump();
                llvm::outs() << "         " << II->getName() << "\n";
                // when the kind of TST is TypeSpec, e.g. std::vector<T>::
                return resolve(QualType(TST->getAsType(), 0), II, arguments);
            }

            case NestedNameSpecifier::SpecifierKind::TypeSpecWithTemplate: {
                llvm::outs() << "------------------ TypeSpecWithTemplate -----------------------\n";
                // when the kind of TST is TypeSpecWithTemplate, e.g. std::vector<T>::template
                // name<U>::
                TST->dump();
                return resolve(QualType(TST->getAsType(), 0), II, arguments);
            }

            default: {
                llvm::outs() << "\n------------------ Unknown -----------------------\n";
                TST->dump();
                std::terminate();
            }
        }
    }

    QualType substitute(ClassTemplateDecl* CTD,
                        const IdentifierInfo* II,
                        ArrayRef<TemplateArgument> arguments = {}) {
        Sema::CodeSynthesisContext context;

        auto args = resolve(arguments);

        context.Entity = CTD;
        context.TemplateArgs = args.data();
        context.Kind = Sema::CodeSynthesisContext::TemplateInstantiation;
        S.pushCodeSynthesisContext(context);

        MultiLevelTemplateArgumentList list;

        auto recordDecl = CTD->getTemplatedDecl();
        auto member = recordDecl->lookup(II).front();

        QualType type;

        if(auto TAD = llvm::dyn_cast<TypeAliasDecl>(member)) {
            type = TAD->getUnderlyingType();
        } else if(auto TD = llvm::dyn_cast<TypedefDecl>(member)) {
            type = TD->getUnderlyingType();
        } else if(auto TATD = llvm::dyn_cast<TypeAliasTemplateDecl>(member)) {
            auto args2 = resolve(arguments);

            context.Entity = TATD;
            context.TemplateArgs = args2.data();
            context.Kind = Sema::CodeSynthesisContext::TypeAliasTemplateInstantiation;
            S.pushCodeSynthesisContext(context);

            MultiLevelTemplateArgumentList list;
            list.addOuterTemplateArguments(TATD, args2, false);

            auto TAD = TATD->getTemplatedDecl();

            type = TAD->getUnderlyingType();
        } else if(auto CTD = llvm::dyn_cast<ClassTemplateDecl>(member)) {
            return substitute(CTD, II, arguments);
        } else {
            member->dump();
            std::terminate();
        }

        list.addOuterTemplateArguments(CTD, args, true);
        return S.SubstType(type, list, {}, {});
    }

    /// typename A<U1>::template B<U2>::template C<U3>::type::
    QualType resolve(const DependentTemplateSpecializationType* DTST, const IdentifierInfo* II) {
        // auto prefix;
        MultiLevelTemplateArgumentList list;
        while(true) {
            // list.addOuterTemplateArguments()
        }
    }

    QualType
        resolve(QualType T, const IdentifierInfo* II, ArrayRef<TemplateArgument> arguments = {}) {
        if(!T->isDependentType() && arguments.size() == 0) {
            // TODO:
        }
        llvm::outs() << "\n";
        T.dump();
        Sema::CodeSynthesisContext context;

        if(auto TTPT = T->getAs<TemplateTypeParmType>()) {
            llvm::outs() << "\n-------------------------------------------------\n";
            T->dump();
            llvm::outs() << "                  \n" << II->getName();

            // e.g. when T is `T`
            // - index: 0
        } else if(auto TST = T->getAs<TemplateSpecializationType>()) {
            auto TemplateName = TST->getTemplateName();
            auto TemplateDecl = TemplateName.getAsTemplateDecl();
            auto TemplatedDecl = TemplateDecl->getTemplatedDecl();

            if(auto CTD = llvm::dyn_cast<ClassTemplateDecl>(TemplateDecl)) {
                return substitute(CTD, II, TST->template_arguments());
            } else {
                TemplateDecl->dump();
                std::terminate();
            }
            auto TemplateArgs = TST->template_arguments();
        } else if(auto DTST = T->getAs<DependentTemplateSpecializationType>()) {
            auto TST = DTST->getQualifier()->getAsType()->getAs<TemplateSpecializationType>();
            auto TemplateName = TST->getTemplateName();
            auto TemplateDecl = TemplateName.getAsTemplateDecl();
            auto TemplatedDecl = TemplateDecl->getTemplatedDecl();

            if(auto CTD = llvm::dyn_cast<ClassTemplateDecl>(TemplateDecl)) {
                auto args = resolve(TST->template_arguments());

                context.Entity = CTD;
                context.TemplateArgs = args.data();
                context.Kind = Sema::CodeSynthesisContext::TemplateInstantiation;
                S.pushCodeSynthesisContext(context);

                MultiLevelTemplateArgumentList list;
                list.addOuterTemplateArguments(CTD, args, true);

                auto recordDecl = CTD->getTemplatedDecl();
                auto member = recordDecl->lookup(DTST->getIdentifier()).front();

                if(auto CTD2 = llvm::dyn_cast<ClassTemplateDecl>(member)) {
                    auto args2 = resolve(DTST->template_arguments());

                    context.Entity = CTD2;
                    context.TemplateArgs = args2.data();
                    context.Kind = Sema::CodeSynthesisContext::TemplateInstantiation;
                    S.pushCodeSynthesisContext(context);

                    MultiLevelTemplateArgumentList list;
                    list.addOuterTemplateArguments(CTD2, args2, true);
                    list.addOuterTemplateArguments(CTD, args, true);

                    auto CRD = CTD2->getTemplatedDecl();
                    return S.SubstType(
                        llvm::dyn_cast<TypeAliasDecl>(CRD->lookup(II).front())->getUnderlyingType(),
                        list,
                        {},
                        {});

                } else if(auto TATD = llvm::dyn_cast<TypeAliasTemplateDecl>(member)) {
                    auto args2 = resolve(arguments);

                    context.Entity = TATD;
                    context.TemplateArgs = args2.data();
                    context.Kind = Sema::CodeSynthesisContext::TypeAliasTemplateInstantiation;
                    S.pushCodeSynthesisContext(context);

                    MultiLevelTemplateArgumentList list;
                    list.addOuterTemplateArguments(TATD, args2, false);
                    list.addOuterTemplateArguments(CTD, args, true);

                    auto TAD = TATD->getTemplatedDecl();
                    return S.SubstType(TAD->getUnderlyingType(), list, {}, {});
                } else {
                    member->dump();
                    std::terminate();
                }
            } else {
                T->dump();
                std::terminate();
            }

            // S.SubstType()
        }
    }
};

class DependentNameResolverV2 {
public:
    Sema& S;
    ASTContext& Ctx;

    std::vector<std::pair<Decl*, std::vector<TemplateArgument>>*> arguments;

public:
    DependentNameResolverV2(ASTContext& Ctx, Sema& S) : Ctx(Ctx), S(S) {}

    std::vector<TemplateArgument> resolve(llvm::ArrayRef<TemplateArgument> arguments) {
        std::vector<TemplateArgument> result;
        for(auto arg: arguments) {
            if(arg.getKind() == TemplateArgument::ArgKind::Type) {
                // check whether it is a TemplateTypeParmType.
                if(auto type = llvm::dyn_cast<TemplateTypeParmType>(arg.getAsType())) {
                    const TemplateTypeParmDecl* param = type->getDecl();
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

    QualType dealias(QualType type) {
        if(auto DNT = type->getAs<TemplateSpecializationType>()) {
            return QualType(DNT, 0);
        } else if(auto DTST = type->getAs<DependentTemplateSpecializationType>()) {
            auto NNS = NestedNameSpecifier::Create(
                Ctx,
                nullptr,
                false,
                dealias(QualType(DTST->getQualifier()->getAsType(), 0)).getTypePtr());
            return Ctx.getDependentTemplateSpecializationType(DTST->getKeyword(),
                                                              NNS,
                                                              DTST->getIdentifier(),
                                                              resolve(DTST->template_arguments()));
        } else {
            return type;
        }
    }

    QualType resolve(QualType type) {

        while(true) {
            // llvm::outs() <<
            // "--------------------------------------------------------------------\n";
            // type.dump();

            MultiLevelTemplateArgumentList list;
            if(auto DNT = type->getAs<DependentNameType>()) {
                type = resolve(resolve(DNT->getQualifier(), DNT->getIdentifier()));
                for(auto begin = arguments.rbegin(), end = arguments.rend(); begin != end;
                    ++begin) {
                    list.addOuterTemplateArguments((*begin)->first, (*begin)->second, true);
                }
                type = S.SubstType(dealias(type), list, {}, {});
                arguments.clear();

            } else if(auto DTST = type->getAs<DependentTemplateSpecializationType>()) {
                auto ND = resolve(DTST->getQualifier(), DTST->getIdentifier());
                if(auto TATD = llvm::dyn_cast<TypeAliasTemplateDecl>(ND)) {
                    auto args = resolve(DTST->template_arguments());

                    Sema::CodeSynthesisContext context;
                    context.Entity = TATD;
                    context.Kind = Sema::CodeSynthesisContext::TypeAliasTemplateInstantiation;
                    context.TemplateArgs = args.data();
                    S.pushCodeSynthesisContext(context);

                    list.addOuterTemplateArguments(TATD, args, true);
                    for(auto begin = arguments.rbegin(), end = arguments.rend(); begin != end;
                        ++begin) {
                        list.addOuterTemplateArguments((*begin)->first, (*begin)->second, true);
                    }

                    // llvm::outs() << "before:
                    // ----------------------------------------------------------------\n";
                    // TATD->getTemplatedDecl()->getUnderlyingType().dump();
                    type = dealias(TATD->getTemplatedDecl()->getUnderlyingType());
                    // llvm::outs() << "arguments:
                    // -------------------------------------------------------------\n";
                    // list.dump();
                    type = S.SubstType(type, list, {}, {});
                    // type.dump();
                    arguments.clear();

                } else {
                    ND->dump();
                    std::terminate();
                }
                // return resolve(DTST);
            } else if(auto LRT = type->getAs<LValueReferenceType>()) {
                type = Ctx.getLValueReferenceType(resolve(LRT->getPointeeType()));
            } else {
                return type;
            }
        }
    }

    QualType resolve(NamedDecl* ND) {
        if(auto TD = llvm::dyn_cast<TypedefDecl>(ND)) {
            return TD->getUnderlyingType();
        } else if(auto TAD = llvm::dyn_cast<TypeAliasDecl>(ND)) {
            return TAD->getUnderlyingType();
        } else {
            ND->dump();
            std::terminate();
        }
    }

    NamedDecl* resolve(const NestedNameSpecifier* NNS, const IdentifierInfo* II) {
        switch(NNS->getKind()) {
            // prefix is an identifier, e.g. <...>::name::
            case NestedNameSpecifier::SpecifierKind::Identifier: {
                return lookup(resolve(resolve(NNS->getPrefix(), NNS->getAsIdentifier())), II);
            }

            // prefix is a type, e.g. <...>::typename name::
            case NestedNameSpecifier::SpecifierKind::TypeSpec:
            case NestedNameSpecifier::SpecifierKind::TypeSpecWithTemplate: {
                return lookup(QualType(NNS->getAsType(), 0), II);
            }

            default: {
                NNS->dump();
                std::terminate();
            }
        }
    }

    NamedDecl* lookup(QualType Type, const IdentifierInfo* Name) {
        NamedDecl* TemplateDecl;
        ArrayRef<TemplateArgument> arguments;

        llvm::outs() << "--------------------------------------------------------------------\n";
        Type.dump();
        Type->getCanonicalTypeInternal();
        if(auto TTPT = Type->getAs<TemplateTypeParmType>()) {
            Type->dump();
            std::terminate();
        } else if(auto TST = Type->getAs<TemplateSpecializationType>()) {
            auto TemplateName = TST->getTemplateName();
            TemplateDecl = TemplateName.getAsTemplateDecl();
            arguments = TST->template_arguments();
        } else if(auto DTST = Type->getAs<DependentTemplateSpecializationType>()) {
            TemplateDecl = resolve(DTST->getQualifier(), DTST->getIdentifier());
            arguments = DTST->template_arguments();
        } else if(auto RT = Type->getAs<RecordType>()) {
            return RT->getDecl()->lookup(Name).front();
        } else {
            Type->dump();
            std::terminate();
        }

        this->arguments.push_back(
            new std::pair<Decl*, std::vector<TemplateArgument>>{TemplateDecl, resolve(arguments)});

        NamedDecl* result;

        Sema::CodeSynthesisContext context;
        context.Entity = TemplateDecl;
        context.Kind = Sema::CodeSynthesisContext::TemplateInstantiation;
        context.TemplateArgs = this->arguments.back()->second.data();
        S.pushCodeSynthesisContext(context);

        if(auto CTD = llvm::dyn_cast<ClassTemplateDecl>(TemplateDecl)) {
            llvm::outs()
                << "--------------------------------------------------------------------\n";
            llvm::SmallVector<ClassTemplatePartialSpecializationDecl*> paritals;
            CTD->getPartialSpecializations(paritals);

            for(auto partial: paritals) {
                partial->getInjectedSpecializationType().dump();
            }
            llvm::outs()
                << "--------------------------------------------------------------------\n";
            // CTD->findPartialSpecialization()
            auto partial = CTD->findPartialSpecialization(Type);
            if(partial) {
                result = partial->lookup(Name).front();
            }

            if(!result) {
                result = CTD->getTemplatedDecl()->lookup(Name).front();
            }
        } else if(auto TATD = llvm::dyn_cast<TypeAliasTemplateDecl>(TemplateDecl)) {
            result = lookup(TATD->getTemplatedDecl()->getUnderlyingType(), Name);
        }

        if(result == nullptr) {
            Type.dump();
            std::terminate();
        }

        return result;
    }
};

}  // namespace clang

#define Traverse(NAME) bool Traverse##NAME(clang::NAME* node)
#define WalkUpFrom(NAME) bool WalkUpFrom##NAME(clang::NAME* node)
#define VISIT(NAME) bool Visit##NAME(clang::NAME* node)
#define VISIT_TYPE(NAME) bool Visit##NAME(clang::NAME node)

class ASTVistor : public clang::RecursiveASTVisitor<ASTVistor> {
private:
    clang::Preprocessor& preprocessor;
    clang::SourceManager& sourceManager;
    clang::syntax::TokenBuffer& buffer;
    clang::ASTContext& context;
    clang::Sema& sema;
    clang::syntax::TokenBuffer& TB;

public:
    ASTVistor(clang::Preprocessor& preprocessor,
              clang::syntax::TokenBuffer& buffer,
              clang::ASTContext& context,
              clang::Sema& sema,
              clang::syntax::TokenBuffer& TB) :
        preprocessor(preprocessor), sourceManager(preprocessor.getSourceManager()), buffer(buffer),
        context(context), sema(sema), TB(TB) {}

    bool TraverseTranslationUnitDecl(clang::TranslationUnitDecl* decl) {
        for(auto it = decl->decls_begin(), end = decl->decls_end(); it != end; ++it) {
            TraverseDecl(*it);
            if(sourceManager.getFilename(sourceManager.getSpellingLoc(it->getLocation())) ==
               "/usr/include/c++/13/bits/stl_vector.h") {}
        }
        return true;
    }

    VISIT(ClassTemplateDecl) {
        clang::DeclRefExpr* node2;
        if(node->getName() == "vector") {
            node->getLocation().dump(sourceManager);
        }
        return true;
    }
};

class CommentHandler : public clang::CommentHandler {

    bool HandleComment(clang::Preprocessor& PP, clang::SourceRange Comment) override {
        auto& sm = PP.getSourceManager();
        std::string CommentText = std::string(sm.getCharacterData(Comment.getBegin()),
                                              sm.getCharacterData(Comment.getEnd()) -
                                                  sm.getCharacterData(Comment.getBegin()));
        llvm::outs() << "Comment: " << CommentText << "\n";
        return false;
    }
};

int main(int argc, const char** argv) {
    assert(argc == 2 && "Usage: Preprocessor <source-file>");
    llvm::outs() << "running ASTVisitor...\n";

    auto instance = std::make_unique<clang::CompilerInstance>();

    clang::DiagnosticIDs* ids = new clang::DiagnosticIDs();
    clang::DiagnosticOptions* diag_opts = new clang::DiagnosticOptions();
    clang::DiagnosticConsumer* consumer = new clang::IgnoringDiagConsumer();
    clang::DiagnosticsEngine* engine = new clang::DiagnosticsEngine(ids, diag_opts, consumer);
    instance->setDiagnostics(engine);

    auto invocation = std::make_shared<clang::CompilerInvocation>();
    std::vector<const char*> args = {
        "/usr/local/bin/clang++",
        "-Xclang",
        "-no-round-trip-args",
        "-std=c++20",
        argv[1],
    };

    invocation = clang::createInvocation(args, {});
    // clang::CompilerInvocation::CreateFromArgs(*invocation, args, instance->getDiagnostics());
    instance->setInvocation(std::move(invocation));

    if(!instance->createTarget()) {
        llvm::errs() << "Failed to create target\n";
        std::terminate();
    }

    clang::SyntaxOnlyAction action;

    if(!action.BeginSourceFile(*instance, instance->getFrontendOpts().Inputs[0])) {
        llvm::errs() << "Failed to begin source file\n";
        std::terminate();
    }

    auto& pp = instance->getPreprocessor();
    pp.addCommentHandler(new CommentHandler());
    clang::syntax::TokenCollector collector{pp};
    // pp.addCommentHandler(new CommentHandler());

    if(auto error = action.Execute()) {
        llvm::errs() << "Failed to execute action: " << error << "\n";
        std::terminate();
    }

    clang::syntax::TokenBuffer buffer = std::move(collector).consume();
    buffer.indexExpandedTokens();

    auto& sm = instance->getSourceManager();
    auto& fm = instance->getFileManager();

    auto entry = fm.getFileRef("/usr/include/c++/13/vector");
    auto id = sm.translateFile(*entry);
    auto tokens = buffer.spelledTokens(id);

    for(auto token: tokens) {
        llvm::outs() << token.text(sm) << "  ";
        token.location().dump(sm);
    }

    auto tu = instance->getASTContext().getTranslationUnitDecl();
    ASTVistor visitor{instance->getPreprocessor(),
                      buffer,
                      instance->getASTContext(),
                      instance->getSema(),
                      buffer};
    // visitor.TraverseDecl(tu);

    action.EndSourceFile();
};
