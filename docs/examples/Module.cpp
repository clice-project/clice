#include <clang/Basic/Diagnostic.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Frontend/TextDiagnosticPrinter.h>
#include <clang/Sema/Sema.h>
#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/Syntax/Tokens.h>
#include <clang/Lex/PreprocessorOptions.h>
#include <clang/Tooling/DependencyScanning/DependencyScanningTool.h>

class CodeCompleteConsumer : public clang::CodeCompleteConsumer {
private:
    std::shared_ptr<clang::GlobalCodeCompletionAllocator> Allocator;
    clang::CodeCompletionTUInfo CCTUInfo;

public:
    CodeCompleteConsumer() :
        clang::CodeCompleteConsumer(clang::CodeCompleteOptions{}),
        Allocator(std::make_shared<clang::GlobalCodeCompletionAllocator>()), CCTUInfo(Allocator) {}

    clang::CodeCompletionAllocator& getAllocator() override { return *Allocator; }

    clang::CodeCompletionTUInfo& getCodeCompletionTUInfo() override { return CCTUInfo; }

    void ProcessCodeCompleteResults(clang::Sema& S,
                                    clang::CodeCompletionContext Context,
                                    clang::CodeCompletionResult* Results,
                                    unsigned NumResults) override {
        llvm::outs() << NumResults << " code completion results\n";
        auto kind = Context.getKind();
        if(kind == clang::CodeCompletionContext::CCC_DotMemberAccess) {
            auto type = Context.getBaseType();
            type->dump();
        }
        // auto contexts = Context.getVisitedContexts();
        // for(auto c: contexts) {
        //     // llvm::outs() << "   Kind: " << c->getDeclKindName() << "\n";
        //     // for(auto d: c->decls()) {
        //     //     // d->dump();
        //     // }
        // }
        //
        llvm::outs() << "code completion results:\n";
        // switch(Context.getKind()) {
        //     case clang::CodeCompletionContext::CCC_Attribute: {
        //     }
        //     case clang::CodeCompletionContext::CCC_DotMemberAccess: {
        //         const auto type = Context.getBaseType();
        //         if(type->isDependentType()) {
        //
        //            if(const auto dependentType = type->getAs<clang::DependentNameType>()) {
        //                auto qualifers = dependentType->getQualifier();
        //                // qualifers->getKind() -> clang::NestedNameSpecifier::SpecifierKind;
        //                // auto t = qualifers->getAsType();
        //                // TODO: 看是否能根据主模板一路把依赖名替换下去，直到变成非依赖名
        //            } else if(const auto dependentType =
        //                          type->getAs<clang::TemplateSpecializationType>()) {
        //            }
        //        }
        //        break;
        //    }
        //    /* ... */
        //    default: {
        //        llvm::outs() << "   Kind: " << Context.getKind() << "\n";
        //    }
        //}

        for(unsigned i = 0; i < NumResults; ++i) {
            clang::CodeCompletionResult& Result = Results[i];

            switch(Result.Kind) {
                case clang::CodeCompletionResult::RK_Declaration: {
                    llvm::outs() << "   Declaration: ";
                    llvm::outs() << Result.Declaration->getNameAsString() << "\n";
                    break;
                }

                case clang::CodeCompletionResult::RK_Keyword: {
                    llvm::outs() << "   Keyword: ";
                    llvm::outs() << Result.Keyword << "\n";
                    break;
                }

                case clang::CodeCompletionResult::RK_Macro: {
                    llvm::outs() << "   Macro: ";
                    llvm::outs() << Result.Macro->getName() << "\n";
                    break;
                }

                case clang::CodeCompletionResult::RK_Pattern: {
                    llvm::outs() << "   Pattern: ";
                    llvm::outs() << Result.Pattern->getAsString() << "\n";
                    break;
                }
            }
        }
    }

    void ProcessOverloadCandidates(clang::Sema& S,
                                   unsigned CurrentArg,
                                   OverloadCandidate* Candidates,
                                   unsigned NumCandidates,
                                   clang::SourceLocation OpenParLoc,
                                   bool Braced) override {}
};

int main(int argc, const char** argv) {
    assert(argc == 2 && "Usage: Preprocessor <source-file>");
    llvm::outs() << "running ASTVisitor...\n";

    std::string err;
    auto CDB = clang::tooling::CompilationDatabase::loadFromDirectory(
        "/home/ykiko/C++/clice/docs/examples/temp/build",
        err);

    if(!CDB) {
        llvm::errs() << "Failed to load compilation database: " << err << "\n";
        std::terminate();
    }

    using namespace clang::tooling::dependencies;
    DependencyScanningService service{ScanningMode::DependencyDirectivesScan,
                                      ScanningOutputFormat::P1689};
    DependencyScanningTool tool{service};
    auto cmd = CDB->getCompileCommands("/home/ykiko/C++/clice/docs/examples/temp/src/D.cppm")[0];
    auto dep = tool.getP1689ModuleDependencyFile(cmd, cmd.Directory);
    if(auto error = dep.takeError()) {
        llvm::errs() << "Failed to get module dependency file: " << error << "\n";
        std::terminate();
    }
    for(auto& file: dep->Requires) {
        llvm::outs() << "Module Name: " << file.ModuleName << "\n";
    }

    auto instance = std::make_unique<clang::CompilerInstance>();

    clang::DiagnosticIDs* ids = new clang::DiagnosticIDs();
    clang::DiagnosticOptions* diag_opts = new clang::DiagnosticOptions();
    clang::DiagnosticConsumer* consumer = new clang::TextDiagnosticPrinter(llvm::errs(), diag_opts);
    clang::DiagnosticsEngine* engine = new clang::DiagnosticsEngine(ids, diag_opts, consumer);
    instance->setDiagnostics(engine);

    auto invocation = std::make_shared<clang::CompilerInvocation>();
    std::vector<const char*> args{
        "/usr/local/bin/clang++",
        "-std=gnu++20",
        "-x",
        "c++-module",
        "-fmodule-output=/home/ykiko/C++/clice/docs/examples/temp/build/CMakeFiles/test_modules.dir/D.pcm",
        "-fmodule-file=B=/home/ykiko/C++/clice/docs/examples/temp/build/CMakeFiles/test_modules.dir/B.pcm",
        "-fmodule-file=C=/home/ykiko/C++/clice/docs/examples/temp/build/CMakeFiles/test_modules.dir/C.pcm",
        "-fmodule-file=A=/home/ykiko/C++/clice/docs/examples/temp/build/CMakeFiles/test_modules.dir/A.pcm",
        "/home/ykiko/C++/clice/docs/examples/temp/src/D.cppm",
        "-Xclang",
        "-no-round-trip-args",
    };

    invocation = clang::createInvocation(args, {});

    auto& codeCompletionAt = invocation->getFrontendOpts().CodeCompletionAt;
    codeCompletionAt.FileName = "/home/ykiko/C++/clice/docs/examples/temp/src/D.cppm";
    codeCompletionAt.Line = std::stoi(argv[1]);
    codeCompletionAt.Column = std::stoi(argv[2]);

    // clang::CompilerInvocation::CreateFromArgs(*invocation, args, instance->getDiagnostics());
    instance->setInvocation(std::move(invocation));

    instance->setCodeCompletionConsumer(new CodeCompleteConsumer());

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
    buffer.indexExpandedTokens();

    auto tu = instance->getASTContext().getTranslationUnitDecl();
    // tu->dump();
    //  ASTVistor visitor{instance->getPreprocessor(),
    //                    buffer,
    //                    instance->getASTContext(),
    //                    instance->getSema(),
    //                    buffer};
    //  visitor.TraverseDecl(tu);

    action.EndSourceFile();
};

