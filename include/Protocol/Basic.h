#pragma once

#include <Support/Reflection.h>

namespace clice::protocol {

/// range in [-2^31, 2^31- 1]
using integer = std::int32_t;

/// range in [0, 2^31- 1]
using uinteger = std::uint32_t;

using string = std::string_view;

using DocumentUri = std::string;

struct Position {
    uinteger line;
    uinteger character;
};

struct Range {
    Position start;
    Position end;
};

struct TextDocumentItem {
    DocumentUri uri;
    string languageId;
    integer version;
    string text;
};

struct TextDocumentIdentifier {
    DocumentUri uri;
};

CLICE_RECORD(VersionedTextDocumentIdentifier, TextDocumentIdentifier) {
    // The version number of this document.
    //
    // The version number of a document will increase after each change,
    // including undo/redo. The number doesn't need to be consecutive.
    integer version;
};

}  // namespace clice::protocol
