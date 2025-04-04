#pragma once

#include <vector>
#include <string>
#include <cstdint>

#include "Support/Enum.h"
#include "llvm/ADT/StringRef.h"

namespace clice::proto {

using integer = std::int32_t;

/// range in [0, 2^31- 1]
using uinteger = std::uint32_t;

using string = std::string;

using string_literal = llvm::StringLiteral;

template <typename T>
using array = std::vector<T>;

using DocumentUri = std::string;

using URI = std::string;

struct None {};

/// A set of predefined position encoding kinds.
struct PositionEncodingKind : refl::Enum<PositionEncodingKind, false, std::string_view> {
    using Enum::Enum;

    constexpr inline static std::string_view UTF8 = "utf-8";
    constexpr inline static std::string_view UTF16 = "utf-16";
    constexpr inline static std::string_view UTF32 = "utf-32";

    constexpr inline static std::array All = {UTF8, UTF16, UTF32};
};

struct Position {
    /// Line position in a document (zero-based).
    uinteger line;

    /// Character offset on a line in a document (zero-based).
    /// The meaning of this offset is determined by the negotiated
    /// `PositionEncodingKind`.
    uinteger character;

    constexpr friend bool operator== (const Position&, const Position&) = default;
};

struct Range {
    /// The range's start position.
    Position start;

    /// The range's end position.
    Position end;

    constexpr friend bool operator== (const Range&, const Range&) = default;
};

struct Location {
    DocumentUri uri;

    Range range;
};

struct TextEdit {
    /// The range of the text document to be manipulated. To insert
    /// text into a document create a range where start === end.
    Range range;

    // The string to be inserted. For delete operations use an
    // empty string.
    string newText;
};

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

struct TextDocumentPositionParams {
    /// The text document.
    TextDocumentIdentifier textDocument;

    /// The position inside the text document.
    Position position;
};

struct WorkspaceFolder {
    /// The associated URI for this workspace folder.
    URI uri;

    /// The name of the workspace folder. Used to refer to this workspace folder
    /// in the user interface.
    std::string name;
};

struct ClientCapabilities {
    /// General client capabilities.
    struct {
        /// The position encodings supported by the client. Client and server
        /// have to agree on the same position encoding to ensure that offsets
        /// (e.g. character position in a line) are interpreted the same on both
        /// side.
        ///
        /// To keep the protocol backwards compatible the following applies: if
        /// the value 'utf-16' is missing from the array of position encodings
        /// servers can assume that the client supports UTF-16. UTF-16 is
        /// therefore a mandatory encoding.
        ///
        /// If omitted it defaults to ['utf-16'].
        ///
        /// Implementation considerations: since the conversion from one encoding
        /// into another requires the content of the file / line the conversion
        /// is best done where the file is read which is usually on the server
        /// side.
        std::vector<PositionEncodingKind> positionEncodings = {PositionEncodingKind::UTF16};
    } general;
};

struct InitializeParams {
    /// Information about the client.
    struct {
        /// The name of the client as defined by the client.
        std::string name;

        /// The client's version as defined by the client.
        std::string version;
    } clientInfo;

    /// The capabilities provided by the client (editor or tool).
    ClientCapabilities capabilities;

    /// The workspace folders configured in the client when the server starts.
    /// This property is only available if the client supports workspace folders.
    /// It can be `null` if the client supports workspace folders but none are
    /// configured.
    std::vector<WorkspaceFolder> workspaceFolders;
};

}  // namespace clice::proto

namespace clice::proto {

struct TextDocumentParams {
    /// The text document.
    TextDocumentIdentifier textDocument;
};

using SemanticTokensParams = TextDocumentParams;

using FoldingRangeParams = TextDocumentParams;

using DocumentLinkParams = TextDocumentParams;

using DocumentSymbolParams = TextDocumentParams;

enum class SymbolKind {};

struct HeaderContext {
    /// The path of context file.
    std::string file;

    /// The version of context file's AST.
    uint32_t version;

    /// The include location id for further resolving.
    uint32_t include;
};

struct IncludeLocation {
    /// The line of include drective.
    uint32_t line = -1;

    /// The file path of include drective.
    std::string file;
};

struct HeaderContextGroup {
    /// The index path of this header Context.
    std::string indexFile;

    /// The header contexts.
    std::vector<HeaderContext> contexts;
};

struct HeaderContextSwitchParams {
    /// The header file path which wants to switch context.
    std::string header;

    /// The context
    HeaderContext context;
};

}  // namespace clice::proto
