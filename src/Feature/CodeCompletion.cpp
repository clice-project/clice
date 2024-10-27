#include <Basic/SourceCode.h>
#include <Compiler/Compiler.h>
#include <Feature/CodeCompletion.h>

namespace clice::feature {

namespace {

class CodeCompletionCollector final : public clang::CodeCompleteConsumer {
public:
    CodeCompletionCollector(clang::CodeCompleteOptions options) :
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

std::vector<proto::CompletionItem> codeCompletion(Compiler& compiler,
                                                  llvm::StringRef filepath,
                                                  proto::Position position,
                                                  const config::CodeCompletionOption& option) {
    // TODO: decode here.
}

}  // namespace clice::feature
