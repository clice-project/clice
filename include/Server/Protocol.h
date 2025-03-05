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

}  // namespace clice::proto
