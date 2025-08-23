#pragma once

#include "../Basic.h"

namespace clice::proto {

struct DocumentFormattingClientCapabilities {};

using DocumentFormattingOptions = bool;

struct DocumentFormattingParams {
    /// The document to format.
    TextDocumentIdentifier textDocument;
};

struct DocumentRangeFormattingParams {
    /// The document to format.
    TextDocumentIdentifier textDocument;

    /// The range to format
    Range range;
};

struct DocumentRangeFormattingClientCapabilities {};

using DocumentRangeFormattingOptions = bool;

struct DocumentOnTypeFormattingClientCapabilities {};

struct DocumentOnTypeFormattingOptions {};

}  // namespace clice::proto
