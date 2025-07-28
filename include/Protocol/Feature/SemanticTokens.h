#pragma once

#include "../Basic.h"

namespace clice::proto {

struct SemanticTokensClientCapabilities {};

struct SemanticTokensLegend {
    /// The token types a server uses.
    array<string> tokenTypes;

    /// The token modifiers a server uses.
    array<string> tokenModifiers;
};

struct SemanticTokensOptions {
    /// The legend used by the server.
    SemanticTokensLegend legend;

    /// Server supports providing semantic tokens for a specific
    /// range of a document.
    bool range = false;

    /// Server supports providing semantic tokens for a full document.
    bool full = true;
};

struct SemanticTokensParams {
    /// The text document.
    TextDocumentIdentifier textDocument;
};

struct SemanticTokens {};

}  // namespace clice::proto
