#pragma once

#include "Basic/Lifecycle.h"

namespace clice::proto {

struct SemanticTokensParams {
    /// The text document.
    TextDocumentIdentifier textDocument;
};

struct FoldingRangeParams {
    /// The text document.
    TextDocumentIdentifier textDocument;
};

}  // namespace clice::proto
