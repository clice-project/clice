#include <Basic/URI.h>
#include <Basic/SourceCode.h>
#include <Compiler/Compiler.h>
#include <Feature/CodeCompletion.h>

namespace clice::feature {

namespace {

class CodeCompletionCollector final : public clang::CodeCompleteConsumer {
public:
    CodeCompletionCollector(proto::CompletionResult& completions, uint32_t line, uint32_t column) :
        clang::CodeCompleteConsumer({}), completions(completions),
        allocator(new clang::GlobalCodeCompletionAllocator()), info(allocator), line(line),
        column(column) {}

    void ProcessCodeCompleteResults(clang::Sema& sema,
                                    clang::CodeCompletionContext context,
                                    clang::CodeCompletionResult* results,
                                    unsigned count) final {
        auto loc = sema.getPreprocessor().getCodeCompletionLoc();
        for(auto& result: llvm::make_range(results, results + count)) {
            auto& completion = completions.emplace_back();
            switch(result.Kind) {
                case clang::CodeCompletionResult::RK_Declaration: {
                    completion.label = result.Declaration->getDeclName().getAsString();
                    break;
                }
                case clang::CodeCompletionResult::RK_Keyword: {
                    completion.label = result.Keyword;
                    break;
                }
                case clang::CodeCompletionResult::RK_Macro: {
                    completion.label = result.Macro->getName();
                    break;
                }
                case clang::CodeCompletionResult::RK_Pattern: {
                    completion.label = result.Pattern->getTypedText();
                    break;
                }
            }
            completion.textEdit.newText = completion.label;
            completion.textEdit.range = {
                .start = {line - 1, column - 1                                                 },
                .end = {line - 1, static_cast<uint32_t>(column + completion.label.size()) - 1},
            };
            completion.kind = proto::CompletionItemKind::Text;
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
    proto::CompletionResult& completions;
    uint32_t line;
    uint32_t column;
};

}  // namespace

json::Value capability(json::Value clientCapabilities) {
    return json::Object{
        // We don't set `(` etc as allCommitCharacters as they interact
        // poorly with snippet results.
        // See https://github.com/clangd/vscode-clangd/issues/357
        // Hopefully we can use them one day without this side-effect:
        //     https://github.com/microsoft/vscode/issues/42544
        {"resolveProvider",   false                               },
        // We do extra checks, e.g. that > is part of ->.
        {"triggerCharacters", {".", "<", ">", ":", "\"", "/", "*"}},
    };
}

proto::CompletionResult codeCompletion(CompliationParams& compliation,
                                       uint32_t line,
                                       uint32_t column,
                                       llvm::StringRef file,
                                       const config::CodeCompletionOption& option) {
    proto::CompletionResult completions;
    auto consumer = new CodeCompletionCollector(completions, line, column);

    auto info = codeCompleteAt(compliation, line, column, file, consumer);
    if(info) {
        return completions;
    } else {
        std::terminate();
    }
}

}  // namespace clice::feature
