#include <Clang/Clang.h>

// clang 的 CodeCompletion 提供的功能非常有限，无法直接区分当前的

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
        auto contexts = Context.getVisitedContexts();
        for(auto c: contexts) {
            llvm::outs() << "   Kind: " << c->getDeclKindName() << "\n";
        }

        llvm::outs() << "code completion results:\n";
        switch(Context.getKind()) {
            case clang::CodeCompletionContext::CCC_Attribute: {
            }
            case clang::CodeCompletionContext::CCC_DotMemberAccess: {
                const auto type = Context.getBaseType();
                if(type->isDependentType()) {

                    if(const auto dependentType = type->getAs<clang::DependentNameType>()) {
                        auto qualifers = dependentType->getQualifier();
                        // qualifers->getKind() -> clang::NestedNameSpecifier::SpecifierKind;
                        // auto t = qualifers->getAsType();
                        // TODO: 看是否能根据主模板一路把依赖名替换下去，直到变成非依赖名
                    } else if(const auto dependentType = type->getAs<clang::TemplateSpecializationType>()) {
                    }
                }
                break;
            }
            /* ... */
            default: {
                llvm::outs() << "   Kind: " << Context.getKind() << "\n";
            }
        }

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
    assert(argc == 4 && "Usage: CodeCompletion <source-file> <line> <column>");
    llvm::outs() << "running CodeCompletion...\n";

    auto instance = std::make_unique<clang::CompilerInstance>();

    clang::DiagnosticIDs* ids = new clang::DiagnosticIDs();
    clang::DiagnosticOptions* diag_opts = new clang::DiagnosticOptions();
    diag_opts->IgnoreWarnings = true;
    clang::DiagnosticConsumer* consumer = new clang::TextDiagnosticPrinter(llvm::errs(), diag_opts);
    clang::DiagnosticsEngine* engine = new clang::DiagnosticsEngine(ids, diag_opts, consumer);
    instance->setDiagnostics(engine);

    auto invocation = std::make_shared<clang::CompilerInvocation>();
    std::vector<const char*> args = {
        "/home/ykiko/Project/C++/clice/external/llvm/bin/clang++",
        "-Xclang",
        "-no-round-trip-args",
        "-std=c++20",
        "-Wno-everything",
        argv[1],
        "-c",
    };

    invocation = clang::createInvocation(args, {});

    /// NOTICE:
    auto& codeCompletionAt = invocation->getFrontendOpts().CodeCompletionAt;
    codeCompletionAt.FileName = argv[1];
    codeCompletionAt.Line = std::stoi(argv[2]);
    codeCompletionAt.Column = std::stoi(argv[3]);

    instance->setInvocation(std::move(invocation));

    if(!instance->createTarget()) {
        llvm::errs() << "Failed to create target\n";
        std::terminate();
    }

    if(clang::FileManager* manager = instance->createFileManager()) {
        instance->createSourceManager(*manager);
    } else {
        llvm::errs() << "Failed to create file manager\n";
        std::terminate();
    }

    instance->createPreprocessor(clang::TranslationUnitKind::TU_Complete);

    // ASTContent is necessary for SemanticAnalysis
    instance->createASTContext();

    /// NOTICE:
    instance->setCodeCompletionConsumer(new CodeCompleteConsumer());

    clang::SyntaxOnlyAction action;

    if(!action.BeginSourceFile(*instance, instance->getFrontendOpts().Inputs[0])) {
        llvm::errs() << "Failed to begin source file\n";
        std::terminate();
    }

    auto& pp = instance->getPreprocessor();
    pp.setTokenWatcher([&pp](const clang::Token& token) {
        llvm::outs() << "token: " << pp.getSpelling(token) << " kind: " << token.getName() << "\n";
    });

    if(auto error = action.Execute()) {
        llvm::errs() << "Failed to execute action: " << error << "\n";
        std::terminate();
    }

    auto tu = instance->getASTContext().getTranslationUnitDecl();
    tu->dump();

    action.EndSourceFile();
}
