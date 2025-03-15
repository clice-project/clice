#include "AST/SymbolKind.h"
#include "Basic/SourceConverter.h"
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
    CodeCompletionCollector(std::vector<CodeCompletionItem>& completions,
                            uint32_t line,
                            uint32_t column,
                            llvm::StringRef content) :
        clang::CodeCompleteConsumer({}), completions(completions),
        allocator(new clang::GlobalCodeCompletionAllocator()), info(allocator), line(line),
        column(column), content(content) {}

    void ProcessCodeCompleteResults(clang::Sema& sema,
                                    clang::CodeCompletionContext context,
                                    clang::CodeCompletionResult* results,
                                    unsigned count) final {
        // auto loc = sema.getPreprocessor().getCodeCompletionLoc();
        // auto offset = sema.getSourceManager().getFileOffset(loc);
        // auto prefix = CompletionPrefix::from(content, offset);
        //
        // for(auto& result: llvm::make_range(results, results + count)) {
        //    auto& item = completions.emplace_back();
        //    item.kind = proto::CompletionItemKind::Text;
        //    switch(result.Kind) {
        //        case clang::CodeCompletionResult::RK_Declaration: {
        //            item.label = getName(result.Declaration);
        //            item.kind = kindForDecl(result.Declaration);
        //            item.detail = result.Declaration->getNameAsString();
        //            break;
        //        }
        //        case clang::CodeCompletionResult::RK_Keyword: {
        //            item.label = result.Keyword;
        //            item.kind = proto::CompletionItemKind::Keyword;
        //            break;
        //        }
        //        case clang::CodeCompletionResult::RK_Macro: {
        //            item.label = result.Macro->getName();
        //            break;
        //        }
        //        case clang::CodeCompletionResult::RK_Pattern: {
        //            item.kind = proto::CompletionItemKind::Snippet;
        //            item.label = result.Pattern->getTypedText();
        //            break;
        //        }
        //    }
        //    item.textEdit.newText = item.label;
        //    item.textEdit.range = {
        //        .start = {line - 1, static_cast<uint32_t>(column - 1 - prefix.name.size())},
        //        .end = {line - 1, static_cast<uint32_t>(column + item.label.size()) - 1 },
        //    };
        //}
    }

    clang::CodeCompletionAllocator& getAllocator() final {
        return *allocator;
    }

    clang::CodeCompletionTUInfo& getCodeCompletionTUInfo() final {
        return info;
    }

private:
    uint32_t line;
    uint32_t column;
    llvm::StringRef content;
    std::shared_ptr<clang::GlobalCodeCompletionAllocator> allocator;
    clang::CodeCompletionTUInfo info;
    std::vector<CodeCompletionItem>& completions;
};

}  // namespace

std::vector<CodeCompletionItem> codeCompletion(CompilationParams& params,
                                               const config::CodeCompletionOption& option) {
    std::vector<CodeCompletionItem> completions;
    auto consumer =
        new CodeCompletionCollector(completions, params.line, params.column, params.content);

    if(auto info = compile(params, consumer)) {
        for(auto& item: completions) {}
        return completions;
    } else {
        std::abort();
    }
}

}  // namespace clice::feature
