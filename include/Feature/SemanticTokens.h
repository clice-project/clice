#pragma once

#include <Basic/Basic.h>

namespace clice::proto {

// clang-format off
enum class SemanticTokenType : uint8_t {
    #define SEMANTIC_TOKEN_TYPE(name, ...) name,
    #include "SemanticTokens.def"
    #undef SEMANTIC_TOKEN_TYPE
    LAST_TYPE
};

enum class SemanticTokenModifier : uint8_t {
    #define SEMANTIC_TOKEN_MODIFIER(name, ...) name,
    #include "SemanticTokens.def"
    #undef SEMANTIC_TOKEN_MODIFIER
    LAST_MODIFIER
};

struct SemanticTokensLegend {
    std::array<std::string_view, static_cast<int>(SemanticTokenType::LAST_TYPE)> tokenTypes = {
        #define SEMANTIC_TOKEN_TYPE(name, value) value,
        #include "SemanticTokens.def"
    };

    std::array<std::string_view, static_cast<int>(SemanticTokenModifier::LAST_MODIFIER)> tokenModifiers = {
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
    /// TextDocumentIdentifier textDocument;
};

struct SemanticTokens {
    /// The actual tokens.
    std::vector<uinteger> data;
};

}  // namespace clice::proto

namespace clice::feature {
// proto::SemanticTokens semanticTokens(const ParsedAST& AST, llvm::StringRef filename);
}  // namespace clice::feature

