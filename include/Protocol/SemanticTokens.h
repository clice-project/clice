#pragma once

#include "Basic.h"

namespace clice::protocol {

// clang-format off
enum SemanticTokenType : uint8_t {
    #define SEMANTIC_TOKEN_TYPE(name, ...) name,
    #include "SemanticTokens.def"
    #undef SEMANTIC_TOKEN_TYPE
    LAST_TYPE
};

enum SemanticTokenModifier : uint8_t {
    #define SEMANTIC_TOKEN_MODIFIER(name, ...) name,
    #include "SemanticTokens.def"
    #undef SEMANTIC_TOKEN_MODIFIER
    LAST_MODIFIER
};

struct SemanticTokensLegend {
    std::array<std::string_view, SemanticTokenType::LAST_TYPE> tokenTypes = {
        #define SEMANTIC_TOKEN_TYPE(name, value) value,
        #include "SemanticTokens.def"
    };

    std::array<std::string_view, SemanticTokenModifier::LAST_MODIFIER> tokenModifiers = {
        #define SEMANTIC_TOKEN_MODIFIER(name, value) value,
        #include "SemanticTokens.def"
    };
};  // clang-format on

/// Server Capability.
struct SemanticTokensOptions {
    /// The legend used by the server.
    SemanticTokensLegend legend;

    /// Server supports providing semantic tokens for a specific range of a document.
    bool range = false;

    /// Server supports providing semantic tokens for a full document.
    bool full = true;
};

struct SemanticTokensParams {
    /// The text document.
    TextDocumentIdentifier textDocument;
};

struct SemanticTokens {
    /// The actual tokens.
    std::vector<uinteger> data;
};

}  // namespace clice::protocol
