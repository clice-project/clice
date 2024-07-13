#include <Clang/Clang.h>
#include <clang/Lex/PreprocessorOptions.h>

// clang 的 CodeCompletion 提供的功能非常有限，无法获取当前 Completion 的 scope
// 而获取 scope 是非常重要的一个功能，有了它我们可以做更多事情，比如只把 ...sizeof 显示在可变模板参数的语境下
// 又或者根据函数名的语境不同(定义和调用)执行不同的补全逻辑，定义的时候就自动补全参数，调用的时候就不补全
// 我们只能通过一些 hack 的手段来做这个事情了，首先使用 TokenWatcher 可以获取到所有的 Token
// 同样我们可以获取到最终的 tu，

// TODO: 检查 SemaCodeCompletion.cpp，看看能不能把当前代码补全的 scope 拿到，当然在这之前，先尝试前面那种 hack
// 的 方式，看看能不能同样拿到这些信息，反正 token 位置和最后的语法树都是有的 由于要改 clang
// 源码，所以这不是一个高优先级的任务 ...

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
            for(auto d: c->decls()) {
                d->dump();
            }
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

    clang::PreprocessorOptions& popts = invocation->getPreprocessorOpts();
    popts.DetailedRecord = true;

    instance->setInvocation(std::move(invocation));

    if(!instance->createTarget()) {
        llvm::errs() << "Failed to create target\n";
        std::terminate();
    }

    /// NOTICE:
    instance->setCodeCompletionConsumer(new CodeCompleteConsumer());

    clang::SyntaxOnlyAction action;

    if(!action.BeginSourceFile(*instance, instance->getFrontendOpts().Inputs[0])) {
        llvm::errs() << "Failed to begin source file\n";
        std::terminate();
    }

    auto& pp = instance->getPreprocessor();
    pp.setTokenWatcher([&pp](const clang::Token& token) {
        if(!token.isAnnotation()) {
            // llvm::outs() << "token: " << pp.getSpelling(token) << " kind: " << token.getName() << "\n";
        }
    });

    if(auto error = action.Execute()) {
        llvm::errs() << "Failed to execute action: " << error << "\n";
        std::terminate();
    }

    auto tu = instance->getASTContext().getTranslationUnitDecl();
    // tu->dump();

    action.EndSourceFile();
}
