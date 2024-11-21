#pragma once

#include "Location.h"

namespace clice::proto {

struct TextDocumentItem {
    /// The text document's URI.
    DocumentUri uri;

    /// The text document's language identifier.
    string languageId;

    /// The version number of this document (it will strictly increase after each
    /// change, including undo/redo).
    uinteger version;

    /// The content of the opened text document.
    string text;
};

}  // namespace clice::proto
