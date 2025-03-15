#pragma once

#include "Feature/CodeAction.h"
#include "Feature/CodeCompletion.h"
#include "Feature/CodeLens.h"
#include "Feature/DocumentHighlight.h"
#include "Feature/DocumentLink.h"
#include "Feature/FoldingRange.h"
#include "Feature/Formatting.h"
#include "Feature/Hover.h"
#include "Feature/SemanticTokens.h"
#include "Feature/SignatureHelp.h"

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
};

constexpr bool operator== (const proto::Position& lhs, const proto::Position rhs) {
    return lhs.character == rhs.character && lhs.line == rhs.line;
}

constexpr auto operator<=> (const proto::Position& lhs, const proto::Position rhs) {
    return std::tie(lhs.line, lhs.character) <=> std::tie(rhs.line, rhs.character);
}

struct Range {
    /// The range's start position.
    Position start;

    /// The range's end position.
    Position end;
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

struct TextDocumentSyncKind : refl::Enum<TextDocumentSyncKind, false, std::uint8_t> {
    using Enum::Enum;

    enum Kind : std::uint8_t {
        /// Documents should not be synced at all.
        None = 0,

        /// Documents are synced by always sending the full content of the document.
        Full = 1,

        /// Documents are synced by sending the full content on open. After that
        /// only
        /// incremental updates to the document are sent.
        Incremental = 2,
    };
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

struct VersionedTextDocumentIdentifier {
    /// The text document's URI.
    DocumentUri uri;
    /// The version number of this document.
    ///
    /// The version number of a document will increase after each change,
    /// including undo/redo. The number doesn't need to be consecutive.
    integer version;
};

/// An event describing a change to a text document. If only a text is provided
/// it is considered to be the full content of the document.
struct TextDocumentContentChangeEvent {
    /// The range of the document that changed.
    Range range;

    /// The new text for the provided range.
    string text;
};

struct DidChangeTextDocumentParams {
    /// The document that did change. The version number points
    /// to the version after all provided content changes have
    /// been applied.
    VersionedTextDocumentIdentifier textDocument;

    /// The actual content changes. The content changes describe single state
    /// changes to the document. So if there are two content changes c1 (at
    /// array index 0) and c2 (at array index 1) for a document in state S then
    /// c1 moves the document from S to S' and c2 from S' to S''. So c1 is
    /// computed on the state S and c2 is computed on the state S'.
    ///
    /// To mirror the content of a document using change events use the following
    /// approach:
    ///  - start with the same initial content
    ///  - apply the 'textDocument/didChange' notifications in the order you
    ///  receive them.
    ///  - apply the `TextDocumentContentChangeEvent`s in a single notification
    ///  in the order you receive them.
    std::vector<TextDocumentContentChangeEvent> contentChanges;
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

namespace clice::proto {

struct WorkspaceFolder {
    /// The associated URI for this workspace folder.
    URI uri;

    /// The name of the workspace folder. Used to refer to this workspace folder
    /// in the user interface.
    std::string name;
};

struct DidChangeWatchedFilesParams {};

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

struct SemanticTokensOptions {
    /// The legend used by the server.
    struct SemanticTokensLegend {
        /// The token types a server uses.
        std::vector<std::string> tokenTypes;

        /// The token modifiers a server uses.
        std::vector<std::string> tokenModifiers;
    } legend;

    /// Server supports providing semantic tokens for a specific range
    /// of a document.
    bool range = false;

    /// Server supports providing semantic tokens for a full document.
    bool full = true;
};

struct SemanticTokens {
    /// The actual tokens.
    std::vector<std::uint32_t> data;
};

/// A set of predefined range kinds.
enum class FoldingRangeKind {
    /// Folding range for a comment.
    Comment,

    /// Folding range for imports or includes.
    Imports,

    /// Folding range for a region.
    Region,
};

/// Represents a folding range. To be valid, start and end line must be bigger
/// than zero and smaller than the number of lines in the document. Clients
/// are free to ignore invalid ranges.
struct FoldingRange {
    /// The zero-based start line of the range to fold. The folded area starts
    /// after the line's last character. To be valid, the end must be zero or
    /// larger and smaller than the number of lines in the document.
    uint32_t startLine;

    /// The zero-based character offset from where the folded range starts. If
    /// not defined, defaults to the length of the start line.
    std::optional<uint32_t> startCharacter;

    /// The zero-based end line of the range to fold. The folded area ends with
    /// the line's last character. To be valid, the end must be zero or larger
    /// and smaller than the number of lines in the document.
    uint32_t endLine;

    /// The zero-based character offset before the folded range ends. If not
    /// defined, defaults to the length of the end line.
    std::optional<uint32_t> endCharacter;

    /// Describes the kind of the folding range such as `comment` or `region`.
    /// The kind is used to categorize folding ranges and used by commands like
    /// 'Fold all comments'. See [FoldingRangeKind](#FoldingRangeKind) for an
    /// enumeration of standardized kinds.
    FoldingRangeKind kind;

    /// The text that the client should show when the specified range is
    /// collapsed. If not defined or not supported by the client, a default
    /// will be chosen by the client.
    ///
    /// @since 3.17.0 - proposed
    std::optional<std::string> collapsedText;
};

/// Server Capability.
struct ServerCapabilities {
    /// The position encoding the server picked from the encodings offered
    /// by the client via the client capability `general.positionEncodings`.
    ///
    /// If the client didn't provide any position encodings the only valid
    /// value that a server can return is 'utf-16'.
    ///
    /// If omitted it defaults to 'utf-16'.
    PositionEncodingKind positionEncoding = PositionEncodingKind::UTF16;

    /// Defines how text documents are synced. Is either a detailed structure
    /// defining each notification or for backwards compatibility the
    /// TextDocumentSyncKind number. If omitted it defaults to
    /// `TextDocumentSyncKind.None`.
    TextDocumentSyncKind textDocumentSync = TextDocumentSyncKind::None;

    /// The server provides go to declaration support.
    bool declarationProvider = true;

    /// The server provides goto definition support.
    bool definitionProvider = true;

    /// The server provides goto type definition support.
    bool typeDefinitionProvider = true;

    /// The server provides goto implementation support.
    bool implementationProvider = true;

    /// The server provides find references support.
    bool referencesProvider = true;

    /// The server provides call hierarchy support.
    bool callHierarchyProvider = true;

    /// The server provides type hierarchy support.
    bool typeHierarchyProvider = true;

    /// The server provides semantic tokens support.
    SemanticTokensOptions semanticTokensProvider;

    /// The server provides folding provider support.
    bool foldingRangeProvider = true;
};

struct InitializeResult {
    /// The capabilities the language server provides.
    ServerCapabilities capabilities;

    /// Information about the server.
    struct {
        /// The name of the server as defined by the server.
        std::string name;

        /// The server's version as defined by the server.
        std::string version;
    } serverInfo;
};

struct InitializedParams {};

}  // namespace clice::proto
