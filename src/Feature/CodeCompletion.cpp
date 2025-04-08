#include "AST/Utility.h"
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

    static CompletionPrefix from(llvm::StringRef content, std::uint32_t offset) {
        assert(offset <= content.size());
        CompletionPrefix prefix;

        llvm::StringRef rest = content.take_front(offset);

        // Consume the unqualified name. We only handle ASCII characters.
        // isAsciiIdentifierContinue will let us match "0invalid", but we don't mind.
        while(!rest.empty() && clang::isAsciiIdentifierContinue(rest.back())) {
            rest = rest.drop_back();
        }

        prefix.name = content.slice(rest.size(), offset);

        // Consume qualifiers.
        while(rest.consume_back("::") && !rest.ends_with(":")) {
            // reject ::::
            while(!rest.empty() && clang::isAsciiIdentifierContinue(rest.back())) {
                rest = rest.drop_back();
            }
        }

        prefix.qualifier = content.slice(rest.size(), prefix.name.begin() - content.begin());
        return prefix;
    }
};

class CodeCompletionCollector final : public clang::CodeCompleteConsumer {
public:
    CodeCompletionCollector(std::uint32_t offset) :
        clang::CodeCompleteConsumer({}), offset(offset),
        info(std::make_shared<clang::GlobalCodeCompletionAllocator>()) {}

    CompletionItem processCandidate(clang::CodeCompletionResult& candidate) {
        CompletionItem item;

        switch(candidate.Kind) {
            case clang::CodeCompletionResult::RK_Keyword: {
                item.label = candidate.Keyword;
                item.kind = CompletionItemKind::Keyword;
                break;
            }
            case clang::CodeCompletionResult::RK_Pattern: {
                item.label = candidate.Pattern->getAsString();
                item.kind = CompletionItemKind::Snippet;
                break;
            }
            case clang::CodeCompletionResult::RK_Macro: {
                item.label = candidate.Macro->getName();
                item.kind = CompletionItemKind::Unit;
                break;
            }
            case clang::CodeCompletionResult::RK_Declaration: {
                auto decl = candidate.Declaration;
                item.label = getDeclName(decl);
                item.kind = CompletionItemKind::Function;
                break;
            }
        }

        item.deprecated = false;
        item.edit.text = item.label;
        item.edit.range = {
            offset,
            static_cast<uint32_t>(offset + item.label.size()),
        };

        return item;
    }

    void ProcessCodeCompleteResults(clang::Sema& sema,
                                    clang::CodeCompletionContext context,
                                    clang::CodeCompletionResult* candidates,
                                    unsigned count) final {
        for(auto& candidate: llvm::make_range(candidates, candidates + count)) {
            completions.emplace_back(processCandidate(candidate));
        }
    }

    clang::CodeCompletionAllocator& getAllocator() final {
        return info.getAllocator();
    }

    clang::CodeCompletionTUInfo& getCodeCompletionTUInfo() final {
        return info;
    }

    auto dump() {
        return std::move(completions);
    }

private:
    std::uint32_t offset;
    clang::CodeCompletionTUInfo info;
    std::vector<CompletionItem> completions;
};

}  // namespace

std::vector<CompletionItem> codeCompletion(CompilationParams& params,
                                           const config::CodeCompletionOption& option) {
    auto& [file, offset] = params.completion;
    auto consumer = new CodeCompletionCollector(offset);
    if(auto info = compile(params, consumer)) {
        return consumer->dump();
        /// TODO: Handle error here.
    } else {
        return {};
    }
}

}  // namespace clice::feature
