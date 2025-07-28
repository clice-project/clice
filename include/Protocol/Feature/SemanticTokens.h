#pragma once

#include "../Basic.h"

namespace clice::proto {

struct SemanticTokensClientCapabilities {};

struct SemanticTokensOptions {};

struct SemanticTokensParams {
    /// The text document.
    TextDocumentIdentifier textDocument;
};

struct SemanticTokens {};

}  // namespace clice::proto
