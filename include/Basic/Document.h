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

struct TextDocumentIdentifier {
    /// The text document's URI.
    DocumentUri uri;
};

struct VersionedTextDocumentIdentifier {
    /// The text document's URI.
    DocumentUri uri;
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

using MarkupKind = string;

struct MarkupContent {
    /// The type of the Markup.
    MarkupKind kind = "markdown";

    /// The content itself.
    string value;
};

struct DidOpenTextDocumentParams {
    /// The document that was opened.
    TextDocumentItem textDocument;
};

struct DidChangeTextDocumentParams {
    /// The document that did change. The version number points
    /// to the version after all provided content changes have
    /// been applied.
    VersionedTextDocumentIdentifier textDocument;

    /// The actual content changes.
    std::vector<TextDocumentContentChangeEvent> contentChanges;
};

struct DidSaveTextDocumentParams {
    /// The document that was saved.
    TextDocumentIdentifier textDocument;

    /// Optional the content when saved. Depends on the includeText value
    /// when the save notifcation was requested.
    string text;
};

struct DidCloseTextDocumentParams {
    /// The document that was closed.
    TextDocumentIdentifier textDocument;
};

}  // namespace clice::proto
