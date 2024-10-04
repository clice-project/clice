#include <gtest/gtest.h>
#include <Compiler/Compiler.h>

namespace {

using namespace clice;

std::vector<const char*> compileArgs = {
    "clang++",
    "-std=c++20",
    "/home/ykiko/C++/clice2/tests/Source/CodeCompletion/test.cpp",
    "-resource-dir",
    "/home/ykiko/C++/clice2/build/lib/clang/20",
};

class CodeCompletionConsumer : public clang::CodeCompleteConsumer {
public:
    void ProcessCodeCompleteResults(clang::Sema& S,
                                    clang::CodeCompletionContext Context,
                                    clang::CodeCompletionResult* Results,
                                    unsigned NumResults) override {
        for(unsigned i = 0; i < NumResults; ++i) {
            auto str =
                Results[i].CreateCodeCompletionString(S, Context, getAllocator(), getCodeCompletionTUInfo(), true);
            llvm::outs() << str->getAsString() << "\n";
        }
    }

    void ProcessOverloadCandidates(clang::Sema& S,
                                   unsigned CurrentArg,
                                   OverloadCandidate* Candidates,
                                   unsigned NumCandidates,
                                   clang::SourceLocation OpenParLoc,
                                   bool Braced) override {}

    clang::CodeCompletionAllocator& getAllocator() override {
        return *Allocator;
    };

    clang::CodeCompletionTUInfo& getCodeCompletionTUInfo() override {
        return TUInfo;
    }

    CodeCompletionConsumer() :
        clang::CodeCompleteConsumer(clang::CodeCompleteOptions()),
        Allocator(std::make_shared<clang::GlobalCodeCompletionAllocator>()), TUInfo(Allocator) {}

private:
    std::shared_ptr<clang::GlobalCodeCompletionAllocator> Allocator;
    clang::CodeCompletionTUInfo TUInfo;
};

TEST(clice, CodeCompletion) {
    auto invocation = clang::createInvocation(compileArgs, {});

    auto& opt = invocation->getFrontendOpts().CodeCompletionAt;
    opt.FileName = "/home/ykiko/C++/clice2/tests/Source/CodeCompletion/test2.h";
    opt.Line = 2;
    opt.Column = 7;

    auto instance = createInstance(std::move(invocation));
    instance->setCodeCompletionConsumer(new CodeCompletionConsumer());
    auto action = std::make_unique<clang::SyntaxOnlyAction>();

    if(!action->BeginSourceFile(*instance, instance->getFrontendOpts().Inputs[0])) {
        llvm::errs() << "Failed to begin source file\n";
        std::terminate();
    }

    if(auto error = action->Execute()) {
        llvm::errs() << "Failed to execute action: " << error << "\n";
        std::terminate();
    }

    instance->getASTContext().getTranslationUnitDecl()->dump();
}

}  // namespace
