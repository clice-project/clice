#pragma once

#include <Support/Reflection.h>

namespace clice::proto {

/// range in [-2^31, 2^31- 1]
using integer = std::int32_t;

/// range in [0, 2^31- 1]
using uinteger = std::uint32_t;

using string = std::string_view;

using DocumentUri = std::string;

struct Position {
    uinteger line;
    uinteger character;

    friend std::strong_ordering operator<=> (const Position& lhs, const Position& rhs) = default;
};

struct Range {
    Position start;
    Position end;

    friend std::strong_ordering operator<=> (const Range& lhs, const Range& rhs) = default;
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

}  // namespace clice::proto
