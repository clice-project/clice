#pragma once

#include "Feature/Lookup.h"
#include "Feature/DocumentHighlight.h"
#include "Feature/DocumentLink.h"
#include "Feature/Hover.h"
#include "Feature/CodeLens.h"
#include "Feature/FoldingRange.h"
#include "Feature/DocumentSymbol.h"
#include "Feature/SemanticTokens.h"
#include "Feature/InlayHint.h"
#include "Feature/CodeCompletion.h"
#include "Feature/SignatureHelp.h"
#include "Feature/CodeAction.h"
#include "Feature/Formatting.h"

#include "Support/Support.h"

namespace clice::proto {

enum class TextDocumentSyncKind {
    None = 0,
    Full = 1,
    Incremental = 2,
};

struct ClientInfo {
    /// The name of the client as defined by the client.
    string name;
    /// The client's version as defined by the client.
    string version;
};

struct DidChangeWatchedFilesClientCapabilities {
    /// Did change watched files notification supports dynamic registration.
    /// Please note that the current protocol doesn't support static
    /// configuration for file changes from the server side.
    bool dynamicRegistration = false;
};

struct ClientCapabilities {
    ///  Workspace specific client capabilities.
    struct {
        /// Capabilities specific to the `workspace/didChangeWatchedFiles`
        /// notification.
        DidChangeWatchedFilesClientCapabilities didChangeWatchedFiles;
    } workspace;
};

struct FileSystemWatcher {
    /// The glob pattern to watch.
    std::string globPattern;

    enum WatchKind {
        Create = 1,
        Change = 2,
        Delete = 4,
    };

    /// The kind of events of interest.
    int kind = WatchKind::Create | WatchKind::Change | WatchKind::Delete;
};

struct DidChangeWatchedFilesRegistrationOptions {
    /// The watchers to register.
    std::vector<FileSystemWatcher> watchers;
};

enum class FileChangeType {
    /// The file got created.
    Created = 1,

    /// The file got changed.
    Changed = 2,

    /// The file got deleted.
    Deleted = 3,
};

struct FileEvent {
    /// The file's URI.
    string uri;

    /// The change type.
    FileChangeType type;
};

struct DidChangeWatchedFilesParams {
    /// The actual file events.
    std::vector<FileEvent> changes;
};

struct Workplace {
    /// The associated URI for this workspace folder.
    string uri;

    /// The name of the workspace folder. Used to refer to this
    /// workspace folder in the user interface.
    string name;
};

struct None {};

struct InitializeParams {
    /// Information about the client
    ClientInfo clientInfo;

    /// The locale the client is currently showing the user interface
    /// in. This must not necessarily be the locale of the operating
    /// system.
    ///
    /// Uses IETF language tags as the value's syntax.
    /// (See https://en.wikipedia.org/wiki/IETF_language_tag)
    string locale;

    /// The capabilities provided by the client (editor or tool).
    ClientCapabilities capabilities;

    /// The workspace folders configured in the client when the server starts.
    /// This property is only available if the client supports workspace folders.
    /// It can be `null` if the client supports workspace folders but none are
    /// configured.
    std::vector<Workplace> workspaceFolders;
};

struct ServerCapabilities {
    std::string_view positionEncoding = "utf-16";
    TextDocumentSyncKind textDocumentSync = TextDocumentSyncKind::Full;
    SemanticTokensOptions semanticTokensProvider;
};

struct InitializeResult {
    ServerCapabilities capabilities;

    struct {
        std::string_view name = "clice";
        std::string_view version = "0.0.1";
    } serverInfo;
};

struct InitializedParams {};

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
