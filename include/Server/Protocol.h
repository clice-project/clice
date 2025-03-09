#pragma once

#include "Basic/Lifecycle.h"

namespace clice::proto {

struct TextDocumentParams {
    /// The text document.
    TextDocumentIdentifier textDocument;
};

enum class SemanticTokenTypes {
    Namespace,
    Type,
    Class,
    Enum,
    Interface,
    Struct,
    TypeParameter,
    Parameter,
    Variable,
    Property,
    EnumMember,
    Event,
    Function,
    Method,
    Macro,
    Keyword,
    Modifier,
    Comment,
    String,
    Number,
    Regexp,
    Operator,
    Decorator
};

using SemanticTokensParams = TextDocumentParams;

using FoldingRangeParams = TextDocumentParams;

struct HeaderContext {
    /// The path of context file.
    std::string file;

    /// The version of context file's AST.
    uint32_t version;

    /// The include location id for further resolving.
    uint32_t include;
};

struct IncludeLocation {
    /// The line of include drective.
    uint32_t line = -1;

    /// The file path of include drective.
    std::string file;
};

struct HeaderContextGroup {
    /// The index path of this header Context.
    std::string indexFile;

    /// The header contexts.
    std::vector<HeaderContext> contexts;
};

struct HeaderContextSwitchParams {
    /// The header file path which wants to switch context.
    std::string header;

    /// The context
    HeaderContext context;
};

}  // namespace clice::proto
