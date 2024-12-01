#pragma once

#include <Basic/Document.h>

namespace clice::proto {

// clang-format off
enum class SemanticTokenType : uint8_t {
    #define SEMANTIC_TOKEN_TYPE(name, ...) name,
    #include "SemanticTokens.def"
    #undef SEMANTIC_TOKEN_TYPE
    Invalid,
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
    SemanticTokensLegend legend = {};

    /// The grammar of C++ is highly context-sensitive, so we only provide semantic tokens for
    /// the whole document.
    bool range = false;

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

}  // namespace clice::proto

namespace clice::feature {

/// FIXME:
proto::SemanticTokens semanticTokens(ASTInfo& compiler, llvm::StringRef filename);

}  // namespace clice::feature

