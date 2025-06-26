#include "AST/Utility.h"
#include "AST/SymbolKind.h"
#include "Compiler/Compilation.h"
#include "Feature/CodeCompletion.h"
#include "Support/FuzzyMatcher.h"
#include "clang/Sema/Sema.h"
#include "clang/Lex/Preprocessor.h"
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

    void initCompletionInfo(clang::Sema& sema) {
        if(init) {
            return;
        }

        auto& PP = sema.getPreprocessor();
        auto& SM = sema.getSourceManager();
        auto loc = PP.getCodeCompletionLoc();
        auto content = SM.getBufferData(SM.getFileID(loc));

        prefix = CompletionPrefix::from(content, offset);
        println("{} {}", prefix.qualifier, prefix.name);

        macther = FuzzyMatcher(prefix.name);

        editRange = {offset, offset};

        /// FIXME: consume the prefix of completion prefix, because we may complete
        /// full qualified name.
        assert(editRange.begin > 0);

        while(clang::isAsciiIdentifierContinue(content[editRange.begin - 1])) {
            editRange.begin -= 1;
        }

        if(editRange.end < content.size()) {
            while(clang::isAsciiIdentifierContinue(content[editRange.end])) {
                editRange.end += 1;
            }
        }

        init = true;
    }

    bool filter(clang::CodeCompletionResult& result) {
        // Class members that are shadowed by subclasses are usually noise.
        if(result.Hidden && result.Declaration && result.Declaration->isCXXClassMember()) {
            return true;
        }

        /// TODO: Skip some rarely use functions like destructors?

        /// TODO: Skip injected class name.
        return false;
    }

    void ProcessCodeCompleteResults(clang::Sema& sema,
                                    clang::CodeCompletionContext context,
                                    clang::CodeCompletionResult* candidates,
                                    unsigned candidates_count) final {
        // Results from recovery mode are generally useless, and the callback after
        // recovery (if any) is usually more interesting. To make sure we handle the
        // future callback from sema, we just ignore all callbacks in recovery mode,
        // as taking only results from recovery mode results in poor completion
        // results.
        // FIXME: in case there is no future sema completion callback after the
        // recovery mode, we might still want to provide some results (e.g. trivial
        // identifier-based completion).
        if(context.getKind() == clang::CodeCompletionContext::CCC_Recovery) {
            return;
        }

        if(candidates_count == 0) {
            return;
        }

        initCompletionInfo(sema);
        /// FIXME: check Sema may run multiple times.

        for(auto& candidate: llvm::make_range(candidates, candidates + candidates_count)) {
            CompletionItem item;

            switch(candidate.Kind) {
                case clang::CodeCompletionResult::RK_Keyword: {
                    item.label = candidate.Keyword;
                    item.kind = CompletionItemKind::Keyword;
                    break;
                }
                case clang::CodeCompletionResult::RK_Pattern: {
                    item.label = candidate.Pattern->getAllTypedText();
                    item.kind = CompletionItemKind::Snippet;

                    /// FIXME: Add an option to enable snippet or not.
                    /// If enable snippet we will render placeholders in CodeCompletionString
                    /// ourselves. e.g. delete <#expression#>. But for label and ranking
                    /// we just use all typed text.
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

            /// Fliter rubbish candidates.
            if(auto score = macther->match(item.label)) {
                item.score = *score;
                item.deprecated = false;
                item.edit.text = item.label;
                item.edit.range = editRange;
                completions.emplace_back(item);
            } else {
                println("filter {}", item.label);
            }
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
    clang::ASTContext* Ctx;
    bool init = false;
    std::uint32_t offset;
    LocalSourceRange editRange;
    CompletionPrefix prefix;
    std::optional<FuzzyMatcher> macther;
    std::vector<CompletionItem> completions;
    clang::CodeCompletionTUInfo info;
};

}  // namespace

std::vector<CompletionItem> code_complete(CompilationParams& params,
                                          const config::CodeCompletionOption& option) {
    auto& [file, offset] = params.completion;
    auto consumer = new CodeCompletionCollector(offset);
    if(auto info = complete(params, consumer)) {
        return consumer->dump();
        /// TODO: Handle error here.
    } else {
        return {};
    }
}

}  // namespace clice::feature
