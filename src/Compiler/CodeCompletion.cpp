#include <Compiler/CodeComplete.h>
#include <clang/Lex/CodeCompletionHandler.h>

namespace clice {

namespace {

class CodeCompleteConsumer final : public clang::CodeCompleteConsumer {
public:
    CodeCompleteConsumer(clang::CodeCompleteOptions options) :
        clang::CodeCompleteConsumer(options), allocator(new clang::GlobalCodeCompletionAllocator()),
        info(allocator) {
        // TODO:
    }

    void ProcessCodeCompleteResults(clang::Sema& sema,
                                    clang::CodeCompletionContext context,
                                    clang::CodeCompletionResult* results,
                                    unsigned count) final {
        sema.getPreprocessor().getCodeCompletionLoc();
        for(auto& result: llvm::make_range(results, results + count)) {
            llvm::outs() << "Kind: " << result.Kind << "         ";
            switch(result.Kind) {
                case clang::CodeCompletionResult::RK_Declaration: {
                    result.getDeclaration()->dump();
                    break;
                }
                case clang::CodeCompletionResult::RK_Keyword: {
                    llvm::outs() << result.Keyword << "\n";
                    break;
                }
                case clang::CodeCompletionResult::RK_Macro: {
                    llvm::outs() << result.Macro << "\n";
                    break;
                }
                case clang::CodeCompletionResult::RK_Pattern: {
                    llvm::outs() << result.Pattern->getAsString() << "\n";
                    break;
                }
            }
        }
    }

    void ProcessOverloadCandidates(clang::Sema& sema,
                                   unsigned CurrentArg,
                                   OverloadCandidate* candidates,
                                   unsigned count,
                                   clang::SourceLocation openParLoc,
                                   bool braced) final {
        llvm::outs() << "ProcessOverloadCandidates\n";
        auto range = llvm::make_range(candidates, candidates + count);
        for(auto& candidate: range) {
            switch(candidate.getKind()) {
                // case
            }
        }
    }

    clang::CodeCompletionAllocator& getAllocator() final {
        return *allocator;
    }

    clang::CodeCompletionTUInfo& getCodeCompletionTUInfo() final {
        return info;
    }

private:
    std::shared_ptr<clang::GlobalCodeCompletionAllocator> allocator;
    clang::CodeCompletionTUInfo info;
};

}  // namespace

}  // namespace clice
