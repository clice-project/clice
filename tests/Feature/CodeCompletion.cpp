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

    Compiler compiler(compileArgs);
    compiler.codeCompletion("/home/ykiko/C++/clice2/tests/Source/CodeCompletion/test.cpp",
                            2,
                            7,
                            new CodeCompletionConsumer());
}

}  // namespace
