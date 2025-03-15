#pragma once

#include "Feature/CodeAction.h"
#include "Feature/CodeCompletion.h"
#include "Feature/CodeLens.h"
#include "Feature/DocumentHighlight.h"
#include "Feature/DocumentLink.h"
#include "Feature/DocumentSymbol.h"
#include "Feature/FoldingRange.h"
#include "Feature/Formatting.h"
#include "Feature/Hover.h"
#include "Feature/InlayHint.h"
#include "Feature/Lookup.h"
#include "Feature/SemanticTokens.h"
#include "Feature/SignatureHelp.h"

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
    std::vector<PositionEncodingKind> positionEncodings = {
        PositionEncodingKind::UTF16};
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
  LookupOptions declarationProvider = {};

  /// The server provides goto definition support.
  LookupOptions definitionProvider = {};

  /// The server provides goto type definition support.
  LookupOptions typeDefinitionProvider = {};

  /// The server provides goto implementation support.
  LookupOptions implementationProvider = {};

  /// The server provides find references support.
  LookupOptions referencesProvider = {};

  /// The server provides call hierarchy support.
  LookupOptions callHierarchyProvider = {};

  /// The server provides type hierarchy support.
  LookupOptions typeHierarchyProvider = {};

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

} // namespace clice::proto
