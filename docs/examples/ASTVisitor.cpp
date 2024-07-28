#include <Clang/Clang.h>
#include <clang/Sema/Lookup.h>

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

    /// for a complex dependent type: `X<...>::name::name2::...::nameN`, we can resolve it recursively.
    /// so we only need to handle the `X<...>::name`, whose prefix is a template specialization type.
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

        } else if(auto aliasTemplateDecl = llvm::dyn_cast<clang::TypeAliasTemplateDecl>(templateDecl)) {
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

class ASTVistor : public clang::RecursiveASTVisitor<ASTVistor> {
private:
    clang::Preprocessor& preprocessor;
    clang::SourceManager& sourceManager;
    clang::syntax::TokenBuffer& buffer;
    clang::ASTContext& context;
    clang::Sema& sema;

public:
    ASTVistor(clang::Preprocessor& preprocessor,
              clang::syntax::TokenBuffer& buffer,
              clang::ASTContext& context,
              clang::Sema& sema) :
        preprocessor(preprocessor), sourceManager(preprocessor.getSourceManager()), buffer(buffer),
        context(context), sema(sema) {}

    bool VisitTypeAliasDecl(clang::TypeAliasDecl* decl) {
        auto& sm = context.getSourceManager();
        if(sm.isInMainFile(decl->getLocation()) && decl->getName() == "type") {
            auto type = decl->getUnderlyingType();
            type.dump();
            llvm::outs() << "----------------------------------------------------------------\n";
            if(auto templateType = type->getAs<clang::DependentNameType>()) {
                DependentShaper shaper{context};
                auto result = shaper.simplify(templateType);
                result->dump();
            }
        }

        return true;
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
        "/home/ykiko/Project/C++/clice/external/llvm/bin/clang++",
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

    clang::syntax::TokenCollector collector{instance->getPreprocessor()};

    if(auto error = action.Execute()) {
        llvm::errs() << "Failed to execute action: " << error << "\n";
        std::terminate();
    }

    clang::syntax::TokenBuffer buffer = std::move(collector).consume();

    auto tu = instance->getASTContext().getTranslationUnitDecl();
    ASTVistor visitor{instance->getPreprocessor(), buffer, instance->getASTContext(), instance->getSema()};
    visitor.TraverseDecl(tu);

    action.EndSourceFile();
};
