#pragma once

#include "Location.h"
#include "Support/Reflection.h"

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

struct TextDocumentIdentifier {
    /// The text document's URI.
    DocumentUri uri;
};

CLICE_RECORD(VersionedTextDocumentIdentifier, TextDocumentIdentifier) {
    /// The version number of this document.
    ///
    /// The version number of a document will increase after each change,
    /// including undo/redo. The number doesn't need to be consecutive.
    integer version;
};

struct TextDocumentContentChangeEvent {
    /// The new text of the whole document.
    string text;
};

struct TextDocumentPositionParams {
    /// The text document.
    TextDocumentIdentifier textDocument;

    /// The position inside the text document.
    Position position;
};

}  // namespace clice::proto
