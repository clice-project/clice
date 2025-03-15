#pragma once

#include <vector>

#include "Support/Enum.h"
#include "llvm/ADT/StringRef.h"

namespace clice::proto {

using integer = std::int32_t;

/// range in [0, 2^31- 1]
using uinteger = std::uint32_t;

using string = std::string;

using string_literal = llvm::StringLiteral;

template <typename T> using array = std::vector<T>;

using DocumentUri = std::string;

using URI = std::string;

struct None {};

/// A set of predefined position encoding kinds.
struct PositionEncodingKind
    : refl::Enum<PositionEncodingKind, false, std::string_view> {
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

constexpr bool operator==(const proto::Position &lhs,
                          const proto::Position rhs) {
  return lhs.character == rhs.character && lhs.line == rhs.line;
}

constexpr auto operator<=>(const proto::Position &lhs,
                           const proto::Position rhs) {
  return std::tie(lhs.line, lhs.character) <=>
         std::tie(rhs.line, rhs.character);
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

struct TextDocumentSyncKind
    : refl::Enum<TextDocumentSyncKind, false, std::uint8_t> {
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

} // namespace clice::proto
