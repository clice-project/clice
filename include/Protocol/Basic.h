#pragma once

#include <vector>
#include <optional>

namespace clice::protocol {

using Integer = int;
using UInteger = unsigned int;
using String = std::string;

class URI {
private:
    String scheme;
    String authority;
    String body;

public:
};

/// Position in a text document expressed as zero-based line and zero-based character offset.
struct Position {
    /// line position in a document (zero-based).
    UInteger line;

    /// character offset on a line in a document (zero-based).
    /// The meaning of this offset is determined by the negotiated `PositionEncodingKind`.
    UInteger character;
};

/// A range in a text document expressed as (zero-based) start and end positions.
struct Range {
    /// The range's start position.
    Position start;

    /// The range's end position.
    Position end;
};

/// An item to transfer a text document from the client to the server.
struct TextDocumentItem {
    /// The text document's URI.
    URI uri;

    /// The text document's language identifier.
    String languageId;

    /// The version number of this document (it will strictly increase after each change, including
    /// undo/redo).
    Integer version;

    /// The content of the opened text document.
    String text;
};

/// Text documents are identified using a URI.
struct TextDocumentIdentifier {
    /// The text document's URI.
    URI uri;
};

/// A textual edit applicable to a text document.
struct TextEdit {
    /// The range of the text document to be manipulated. To insert text into a document create a
    /// range where start === end.
    Range range;

    /// The string to be inserted. For delete operations use an empty string.
    String newText;
};

/// Represents a location inside a resource, such as a line inside a text file.
struct Location {
    URI uri;
    Range range;
};

/// Represents a link between a source and a target location.
struct LocationLink {
    /// Span of the origin of this link.
    Range originSelectionRange;

    /// The target resource identifier of this link.
    URI targetUri;

    /// The full target range of this link. If the target for example is a symbol then target range is the
    /// range enclosing this symbol not including leading/trailing whitespace but everything else
    /// like comments. This information is typically used to highlight the range in the editor.
    Range targetRange;

    /// The range that should be selected and revealed when this link is being followed, e.g the name of a
    /// function. Must be contained by the the `targetRange`. See also `DocumentSymbol#range`
    Range targetSelectionRange;
};

/// Represents a diagnostic, such as a compiler error or warning.
/// Diagnostic objects are only valid in the scope of a resource.
struct Diagnostic {

    /// Represents a related message and source code location for a diagnostic.
    /// This should be used to point to code locations that cause or are related to
    /// a diagnostics, e.g when duplicating a symbol in a scope.
    struct DiagnosticRelatedInformation {
        /// The location of this related diagnostic information.
        Location location;

        /// The message of this related diagnostic information.
        String message;
    };

    /// Structure to capture a description for an error code.
    struct CodeDescription {
        /// An URI where the code is described.
        URI href;
    };

    /// The range at which the message applies.
    Range range;

    /// The diagnostic's severity. Can be omitted. If omitted it is up to the
    /// client to interpret diagnostics as error, warning, info or hint.
    Integer severity;

    /// The diagnostic's code. Can be omitted.
    Integer code;

    /// A human-readable string describing the source of this diagnostic, e.g. 'typescript' or 'super lint'.
    String source;

    /// The diagnostic's message.
    String message;

    /// An array of related diagnostic information, e.g. when symbol-names within a scope collide all
    /// definitions can be marked via this property.
    // TODO: std::vector<DiagnosticRelatedInformation> relatedInformation;
};

}  // namespace clice::protocol
