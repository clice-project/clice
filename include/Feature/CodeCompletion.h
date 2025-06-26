#pragma once

#include <vector>
#include <cstdint>

#include "AST/SourceCode.h"
#include "llvm/ADT/StringRef.h"

namespace clice {

struct CompilationParams;

namespace config {

struct CodeCompletionOption {};

};  // namespace config

namespace feature {

enum class CompletionItemKind {
    None = 0,
    Text,
    Method,
    Function,
    Constructor,
    Field,
    Variable,
    Class,
    Interface,
    Module,
    Property,
    Unit,
    Value,
    Enum,
    Keyword,
    Snippet,
    Color,
    File,
    Reference,
    Folder,
    EnumMember,
    Constant,
    Struct,
    Event,
    Operator,
    TypeParameter
};

struct CompletionItem {
    /// The label displayed when user select the item.
    std::string label;

    std::string detail;

    /// TODO:
    /// std::string sortText;

    CompletionItemKind kind;

    bool deprecated;

    float score;

    struct Edit {
        std::string text;

        LocalSourceRange range;
    } edit;
};

using CodeCompletionResult = std::vector<CompletionItem>;

CodeCompletionResult code_complete(CompilationParams& params,
                                    const config::CodeCompletionOption& option);

}  // namespace feature

}  // namespace clice

