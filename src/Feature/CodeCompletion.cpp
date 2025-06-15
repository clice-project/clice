#include "AST/Utility.h"
#include "AST/SymbolKind.h"
#include "Compiler/Compilation.h"
#include "Feature/CodeCompletion.h"
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
        item.edit.range = editRange;

        return item;
    }

    void initCompletionInfo(clang::Sema& sema) {
        if(init) {
            return;
        }

        auto& PP = sema.getPreprocessor();
        auto& SM = sema.getSourceManager();
        auto loc = PP.getCodeCompletionLoc();
        auto content = SM.getBufferData(SM.getFileID(loc));

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

    void ProcessCodeCompleteResults(clang::Sema& sema,
                                    clang::CodeCompletionContext context,
                                    clang::CodeCompletionResult* candidates,
                                    unsigned count) final {
        initCompletionInfo(sema);

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
    clang::ASTContext* Ctx;
    bool init = false;
    /// TODO:
    /// 1. 计算 token 边界，思考该怎么计算比较合适
    /// 比如 std::vec^ 选择 vector => std::vector
    /// 比如 vec 选择 std::vector => std::vector
    /// 不仅要考虑前缀，也要考虑 token 后缀的替换
    /// 之后记得试一下 clion 里面对 prefix 和 suffix 的处理
    ///
    /// 2. 如果发现用户的光标正在补全头文件，则可以把该行头文件之
    /// 前的代码全 substr 掉，然后再在结果上加几行或者 offset，这样
    /// 可以大大优化补全头文件的速度，毕竟头文件补全只和编译命令有关
    /// 由于 #include 后面可能跟着宏，所以确保出现 <> 或者 "" 再进行这种
    /// 优化
    std::uint32_t offset;
    LocalSourceRange editRange;
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
