#include "AST/SymbolKind.h"
#include "Compiler/Compilation.h"
#include "Feature/CodeCompletion.h"
#include "clang/Sema/CodeCompleteConsumer.h"

namespace clice::feature {

namespace {

struct CompletionPrefix {
    // The unqualified partial name.
    // If there is none, begin() == end() == completion position.
    llvm::StringRef name;

    // The spelled scope qualifier, such as Foo::.
    // If there is none, begin() == end() == name.begin().
    llvm::StringRef qualifier;

    static CompletionPrefix from(llvm::StringRef content, std::size_t offset) {
        assert(offset <= content.size());
        CompletionPrefix result;

        llvm::StringRef rest = content.take_front(offset);

        // Consume the unqualified name. We only handle ASCII characters.
        // isAsciiIdentifierContinue will let us match "0invalid", but we don't mind.
        while(!rest.empty() && clang::isAsciiIdentifierContinue(rest.back())) {
            rest = rest.drop_back();
        }

        result.name = content.slice(rest.size(), offset);

        // Consume qualifiers.
        while(rest.consume_back("::") && !rest.ends_with(":")) {
            // reject ::::
            while(!rest.empty() && clang::isAsciiIdentifierContinue(rest.back())) {
                rest = rest.drop_back();
            }
        }

        result.qualifier = content.slice(rest.size(), result.name.begin() - content.begin());
        return result;
    }
};

class CodeCompletionCollector final : public clang::CodeCompleteConsumer {
public:
    CodeCompletionCollector(std::vector<CodeCompletionItem>& completions) :
        clang::CodeCompleteConsumer({}), completions(completions),
        allocator(new clang::GlobalCodeCompletionAllocator()), info(allocator) {}

    void ProcessCodeCompleteResults(clang::Sema& sema,
                                    clang::CodeCompletionContext context,
                                    clang::CodeCompletionResult* candidates,
                                    unsigned count) final {
        for(auto& candidate: llvm::make_range(candidates, candidates + count)) {
            switch(candidate.Kind) {
                case clang::CodeCompletionResult::RK_Declaration: {
                    break;
                }
                case clang::CodeCompletionResult::RK_Keyword: {
                    break;
                }
                case clang::CodeCompletionResult::RK_Macro: {
                    break;
                }
                case clang::CodeCompletionResult::RK_Pattern: {
                    break;
                }
            }

            println("{}", refl::enum_name(candidate.Kind));
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
    std::vector<CodeCompletionItem>& completions;
};

}  // namespace

std::vector<CodeCompletionItem> codeCompletion(CompilationParams& params,
                                               const config::CodeCompletionOption& option) {
    std::vector<CodeCompletionItem> completions;
    auto consumer = new CodeCompletionCollector(completions);
    if(auto info = compile(params, consumer)) {
        for(auto& item: completions) {}
    }
    return completions;
}

}  // namespace clice::feature
