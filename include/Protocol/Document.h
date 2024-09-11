#pragma once

#include "Basic.h"

namespace clice::protocol {

struct DidOpenTextDocumentParams {
    /// The document that was opened.
    TextDocumentItem textDocument;
};

struct DidChangeTextDocumentParams {
    /// The document that did change. The version number points
    /// to the version after all provided content changes have
    /// been applied.
    VersionedTextDocumentIdentifier textDocument;

    // FIXME:
    // std::vector<string> contentChanges;
};

}  // namespace clice::protocol
