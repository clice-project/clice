#pragma once

#include <array>
#include <vector>
#include <optional>
#include <string_view>

#include "Language/SemanticToken.h"

namespace clice {

// Defined by JSON RPC.
enum class ErrorCode {
    ParseError = -32700,
    InvalidRequest = -32600,
    MethodNotFound = -32601,
    InvalidParams = -32602,
    InternalError = -32603,

    ServerNotInitialized = -32002,
    UnknownErrorCode = -32001,

    // Defined by the protocol.
    RequestCancelled = -32800,
    ContentModified = -32801,
};

struct ResourceOperationKind {
    /// Supports creating new files and folders.
    constexpr inline static std::string_view Create = "create";

    /// Supports renaming existing files and folders.
    constexpr inline static std::string_view Rename = "rename";

    /// Supports deleting existing files and folders.
    constexpr inline static std::string_view Delete = "delete";
};

struct FailureHandlingKind {
    /// Applying the workspace change is simply aborted if one of the changes
    /// provided fails. All operations executed before the failing operation stay
    /// executed.
    constexpr inline static std::string_view Abort = "abort";

    /// All operations are executed transactionally. That is they either all
    /// succeed or no changes at all are applied to the workspace.
    constexpr inline static std::string_view Transactional = "transactional";

    /// If the workspace edit contains only textual file changes they are executed
    /// transactionally. If resource changes (create, rename or delete file) are
    /// part of the change the failure handling strategy is abort.
    constexpr inline static std::string_view TextOnlyTransactional = "textOnlyTransactional";

    /// The client tries to undo the operations already executed. But there is no
    /// guarantee that this is succeeding.
    constexpr inline static std::string_view Undo = "undo";
};

/// A symbol kind.
enum class SymbolKind {
    File = 1,
    Module = 2,
    Namespace = 3,
    Package = 4,
    Class = 5,
    Method = 6,
    Property = 7,
    Field = 8,
    Constructor = 9,
    Enum = 10,
    Interface = 11,
    Function = 12,
    Variable = 13,
    Constant = 14,
    String = 15,
    Number = 16,
    Boolean = 17,
    Array = 18,
    Object = 19,
    Key = 20,
    Null = 21,
    EnumMember = 22,
    Struct = 23,
    Event = 24,
    Operator = 25,
    TypeParameter = 26,
};

struct WorkspaceEditClientCapabilities {
    /// The client supports versioned document changes in `WorkspaceEdit`s
    std::optional<bool> documentChanges;

    /// The resource operations the client supports. Clients should at least
    /// support 'create', 'rename' and 'delete' files and folders.
    /// possible values are in ResourceOperationKind
    std::optional<std::vector<std::string_view>> resourceOperations;

    /// The failure handling strategy of a client if applying the workspace edit
    /// fails.
    /// possible values are in FailureHandlingKind
    std::optional<std::string_view> failureHandling;

    /// Whether the client normalizes line endings to the client specific
    /// setting.
    ///  If set to `true` the client will normalize line ending characters
    /// in a workspace edit to the client specific new line character(s).
    std::optional<bool> normalizesLineEndings;

    struct ChangeAnnotationSupport {
        /// Whether the client groups edits with equal labels into tree nodes,
        /// for instance all edits labelled with "Changes in Strings" would
        /// be a tree node.
        std::optional<bool> groupsOnLabel;
    };

    /// Whether the client in general supports change annotations on text edits,
    /// create file, rename file and delete file changes.
    std::optional<ChangeAnnotationSupport> changeAnnotationSupport;
};

struct DidChangeConfigurationClientCapabilities {
    /// Did change configuration notification supports dynamic registration.
    std::optional<bool> dynamicRegistration;
};

struct DidChangeWatchedFilesClientCapabilities {
    /// Did change watched files notification supports dynamic registration.
    /// Please note that the current protocol doesn't support static
    /// configuration for file changes from the server side.
    std::optional<bool> dynamicRegistration;

    /// Whether the client has support for relative patterns or not.
    std::optional<bool> supportsRelativePattern;
};

struct WorkspaceSymbolClientCapabilities {
    /// Symbol request supports dynamic registration.
    std::optional<bool> dynamicRegistration;

    struct SymbolKind_ {
        /// The symbol kind values the client supports. When this
        /// property exists the client also guarantees that it will
        /// handle values outside its set gracefully and falls back
        /// to a default value when unknown.
        ///
        /// If this property is not present the client only supports
        /// the symbol kinds from `File` to `Array` as defined in
        /// the initial version of the protocol.
        std::vector<SymbolKind> valueSet;
    };

    /// Specific capabilities for the `SymbolKind` in the `workspace/symbol` request.
    std::optional<SymbolKind_> symbolKind;

    struct TagSupport {
        /// The tags supported by the client.
        std::vector<std::string_view> valueSet;
    };

    /// The client supports tags on `SymbolInformation` and `WorkspaceSymbol`.
    /// Clients supporting tags have to handle unknown tags gracefully.
    std::optional<TagSupport> tagSupport;

    struct ResolveSupport {
        /// The properties that a client can resolve lazily. Usually `location.range`.
        std::vector<std::string> properties;
    };

    /// The client support partial workspace symbols. The client will send the
    /// request `workspaceSymbol/resolve` to the server to resolve additional
    /// properties.
    std::optional<ResolveSupport> resolveSupport;
};

struct ExecuteCommandClientCapabilities {
    /// Execute command supports dynamic registration.
    std::optional<bool> dynamicRegistration;
};

struct SemanticTokensWorkspaceClientCapabilities {
    /// Whether the client implementation supports a refresh request sent from
    /// the server to the client.
    ///
    /// Note that this event is global and will force the client to refresh all
    /// semantic tokens currently shown. It should be used with absolute care
    /// and is useful for situation where a server for example detect a project
    /// wide change that requires such a calculation.
    std::optional<bool> refreshSupport;
};

struct CodeLensWorkspaceClientCapabilities {
    /**
     * Whether the client implementation supports a refresh request sent from the
     * server to the client.
     *
     * Note that this event is global and will force the client to refresh all
     * code lenses currently shown. It should be used with absolute care and is
     * useful for situation where a server for example detect a project wide
     * change that requires such a calculation.
     */
    std::optional<bool> refreshSupport;
};

/// Client workspace capabilities specific to inline values.
struct InlineValueWorkspaceClientCapabilities {
    /// Whether the client implementation supports a refresh request sent from
    /// the server to the client.
    ///
    /// Note that this event is global and will force the client to refresh all
    /// inline values currently shown. It should be used with absolute care and
    /// is useful for situation where a server for example detect a project wide
    /// change that requires such a calculation.
    std::optional<bool> refreshSupport;
};

/// Client workspace capabilities specific to inlay hints.
struct InlayHintWorkspaceClientCapabilities {
    /// Whether the client implementation supports a refresh request sent from
    /// the server to the client.
    ///
    /// Note that this event is global and will force the client to refresh all
    /// inlay hints currently shown. It should be used with absolute care and
    /// is useful for situation where a server for example detects a project wide
    /// change that requires such a calculation.
    std::optional<bool> refreshSupport;
};

/// Workspace client capabilities specific to diagnostic pull requests.
struct DiagnosticWorkspaceClientCapabilities {
    /// Whether the client implementation supports a refresh request sent from
    /// the server to the client.
    ///
    /// Note that this event is global and will force the client to refresh all
    /// pulled diagnostics currently shown. It should be used with absolute care
    /// and is useful for situation where a server for example detects a project
    /// wide change that requires such a calculation.
    std::optional<bool> refreshSupport;
};

struct ClientCapabilities {
    struct Workplace {
        /// The client supports applying batch edits
        /// to the workspace by supporting the request
        /// 'workspace/applyEdit'
        std::optional<bool> applyEdit;

        /// Capabilities specific to `WorkspaceEdit`s
        std::optional<WorkspaceEditClientCapabilities> workspaceEdit;

        /// Capabilities specific to the `workspace/didChangeConfiguration` notification.
        std::optional<DidChangeConfigurationClientCapabilities> didChangeConfiguration;

        /// Capabilities specific to the `workspace/didChangeWatchedFiles` notification.
        std::optional<DidChangeWatchedFilesClientCapabilities> didChangeWatchedFiles;

        /// Capabilities specific to the `workspace/symbol` request.
        std::optional<WorkspaceSymbolClientCapabilities> symbol;

        /// Capabilities specific to the `workspace/executeCommand` request.
        std::optional<ExecuteCommandClientCapabilities> executeCommand;

        /// The client has support for workspace folders.
        std::optional<bool> workspaceFolders;

        /// The client supports `workspace/configuration` requests.
        std::optional<bool> configuration;

        /// Capabilities specific to the semantic token requests scoped to the workspace.
        std::optional<SemanticTokensWorkspaceClientCapabilities> semanticTokens;

        ///  Capabilities specific to the code lens requests scoped to the workspace.
        std::optional<CodeLensWorkspaceClientCapabilities> codeLens;

        struct FileOperations {
            /// Whether the client supports dynamic registration for file requests/notifications.
            std::optional<bool> dynamicRegistration;

            /// The client has support for sending didCreateFiles notifications.
            std::optional<bool> didCreate;

            /// The client has support for sending willCreateFiles requests.
            std::optional<bool> willCreate;

            /// The client has support for sending didRenameFiles notifications.
            std::optional<bool> didRename;

            /// The client has support for sending willRenameFiles requests.
            std::optional<bool> willRename;

            /// The client has support for sending didDeleteFiles notifications.
            std::optional<bool> didDelete;

            /// The client has support for sending willDeleteFiles requests.
            std::optional<bool> willDelete;
        };

        /// The client has support for file requests/notifications.
        std::optional<FileOperations> fileOperations;

        /// Client workspace capabilities specific to inline values.
        std::optional<InlineValueWorkspaceClientCapabilities> inlineValue;

        /// Client workspace capabilities specific to inlay hints.
        std::optional<InlayHintWorkspaceClientCapabilities> inlayHint;

        /// Client workspace capabilities specific to diagnostics.
        std::optional<DiagnosticWorkspaceClientCapabilities> diagnostic;
    };

    /// Workspace specific client capabilities.
    std::optional<Workplace> workspace;

    /// Text document specific client capabilities.
    /// TODO: textDocument?: TextDocumentClientCapabilities;

    /// Capabilities specific to the notebook document support.
    /// TODO: notebookDocument?: NotebookDocumentClientCapabilities;

    ///  Window specific client capabilities.
    /// TODO: window: {...}

    /// General client capabilities.
    /// TODO: general: {...}

    /// Experimental client capabilities.
    /// experimental?: LSPAny;
};

/// TODO:
struct URI {};

struct WorkspaceFolder {
    /// The associated URI for this workspace folder.
    URI uri;

    /// The name of the workspace folder. Used to refer to this
    /// workspace folder in the user interface.
    std::string name;
};

struct InitializeParams {
    /// The process Id of the parent process that started the server. Is null if
    /// the process has not been started by another process. If the parent
    /// process is not alive then the server should exit (see exit notification)
    /// its process.
    std::optional<int> processId;

    struct ClientInfo {
        std::string_view name;
        std::optional<std::string_view> version;
    };

    /// Information about the client
    std::optional<ClientInfo> clientInfo;

    /// The locale the client is currently showing the user interface
    /// in. This must not necessarily be the locale of the operating
    /// system.
    ///
    /// Uses IETF language tags as the value's syntax
    /// (See https://en.wikipedia.org/wiki/IETF_language_tag)
    ///
    /// @since 3.16.0
    std::optional<std::string_view> locale;

    /// User provided initialization options.
    /// TODO: initializationOptions?: LSPAny;

    /// The capabilities provided by the client (editor or tool).
    ClientCapabilities capabilities;

    /// The initial trace setting. If omitted trace is disabled ('off').
    /// TODO: trace?: TraceValue;

    /// The workspace folders configured in the client when the server starts.
    /// This property is only available if the client supports workspace folders.
    /// It can be `null` if the client supports workspace folders but none are
    /// configured.
    std::optional<std::vector<WorkspaceFolder>> workspaceFolders;
};

/*===========================================================================//
//                                 RESPONSES                                 //
//===========================================================================*/

struct PositionEncodingKind {
    /// Character offsets count UTF-8 code units (e.g bytes).
    constexpr inline static std::string_view UTF8 = "utf-8";

    /// Character offsets count UTF-16 code units.
    /// This is the default and must always be supported by servers.
    constexpr inline static std::string_view UTF16 = "utf-16";

    /// Character offsets count UTF-32 code units.
    /// Implementation note: these are the same as Unicode code points,
    /// so this `PositionEncodingKind` may also be used for an
    /// encoding-agnostic representation of character offsets.
    constexpr inline static std::string_view UTF32 = "utf-32";
};

/// Defines how the host (editor) should sync document changes to the language server.
enum class TextDocumentSyncKind {
    /// Documents should not be synced at all.
    None = 0,

    /// Documents are synced by always sending the full content of the document.
    Full = 1,

    /// Documents are synced by sending the full content on open. After that only
    /// incremental updates to the document are sent.
    Incremental = 2,
};

/// Completion options.
struct CompletionOptions {
    ///
    /// The additional characters, beyond the defaults provided by the client (typically
    ///[a-zA-Z]), that should automatically trigger a completion request. For example
    ///`.` in JavaScript represents the beginning of an object property or method and is
    /// thus a good candidate for triggering a completion request.
    ///
    /// Most tools trigger a completion request automatically without explicitly
    /// requesting it using a keyboard shortcut (e.g. Ctrl+Space). Typically they
    /// do so when the user starts to type an identifier. For example if the user
    /// types `c` in a JavaScript file code complete will automatically pop up
    /// present `console` besides others as a completion item. Characters that
    /// make up identifiers don't need to be listed here.
    std::array<std::string_view, 7> triggerCharacters = {".", "<", ">", ":", "\"", "/", "*"};

    /// The list of all possible characters that commit a completion. This field
    /// can be used if clients don't support individual commit characters per
    /// completion item. See client capability `completion.completionItem.commitCharactersSupport`.
    ///
    /// If a server provides both `allCommitCharacters` and commit characters on
    /// an individual completion item the ones on the completion item win.
    ///
    /// allCommitCharacters?: string[];
    /// NOTICE: We don't set `(` etc as allCommitCharacters as they interact poorly with snippet results.
    /// See https://github.com/clangd/vscode-clangd/issues/357
    /// Hopefully we can use them one day without this side-effect:
    /// https://github.com/microsoft/vscode/issues/42544

    /// The server provides support to resolve additional information for a completion item.
    bool resolveProvider = false;

    /// The server supports the following `CompletionItem` specific capabilities.
    /// TODO: completionItem?: {...}
};

struct SignatureHelpOptions {
    /// The characters that trigger signature help automatically.
    std::array<std::string_view, 7> triggerCharacters = {"(", ")", "{", "}", "<", ">", ","};

    /// List of characters that re-trigger signature help.
    /// These trigger characters are only active when signature help is already showing.
    ///  All trigger characters are also counted as re-trigger characters.
    std::array<std::string_view, 1> retriggerCharacters = {","};
};

struct CodeLensOptions {
    /// Code lens has a resolve provider as well.
    bool resolveProvider = false;
};

struct DocumentLinkOptions {
    /// Document links have a resolve provider as well.
    bool resolveProvider = false;
};

struct DocumentOnTypeFormattingOptions {
    /// A character on which formatting should be triggered, like `{`.
    std::string_view firstTriggerCharacter = "\n";

    /// More trigger characters.
    /// moreTriggerCharacter?: string[];
};

struct SemanticTokensOptions {
    /// The legend used by the server
    SemanticTokensLegend legend;

    /// Server supports providing semantic tokens for a specific range of a document.
    bool range = false;  // TODO: further check

    struct Full {
        /// Server supports providing semantic tokens for a full document.
        bool delta = true;
    };

    /// Server supports providing semantic tokens for a full document.
    Full full;
};

struct ServerCapabilities {
    /// The position encoding the server picked from the encodings offered
    /// by the client via the client capability `general.positionEncodings`.
    ///
    /// If the client didn't provide any position encodings the only valid
    /// value that a server can return is 'utf-16'.
    ///
    /// If omitted it defaults to 'utf-16'.
    ///
    /// possible values: ['utf-8', 'utf-16', 'utf-32'] in PositionEncodingKind
    std::string_view positionEncoding = PositionEncodingKind::UTF16;

    /// Defines how text documents are synced. Is either a detailed structure
    /// defining each notification or for backwards compatibility the
    /// TextDocumentSyncKind number. If omitted it defaults to `TextDocumentSyncKind.None`.
    TextDocumentSyncKind textDocumentSync = TextDocumentSyncKind::Incremental;

    /// Defines how notebook documents are synced.
    /// TODO: notebookDocumentSync?: NotebookDocumentSyncOptions | NotebookDocumentSyncRegistrationOptions;

    /// The server provides completion support.
    CompletionOptions completionProvider;

    /// The server provides hover support.
    bool hoverProvider = true;

    /// The server provides signature help support.
    SignatureHelpOptions signatureHelpProvider;

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

    /// The server provides document highlight support.
    bool documentHighlightProvider = true;

    /// The server provides document symbol support.
    bool documentSymbolProvider = true;

    /// The server provides code actions. The `CodeActionOptions` return type is
    /// only valid if the client signals code action literal support via the
    /// client capability `textDocument.codeAction.codeActionLiteralSupport`.
    bool codeActionProvider = true;  // TODO: provide CodeActionOptions

    /// The server provides code lens.
    CodeLensOptions codeLensProvider;

    /// The server provides document link support.
    DocumentLinkOptions documentLinkProvider;

    /// The server provides color provider support.
    bool colorProvider = false;  // TODO: check what is colorProvider

    /// The server provides document formatting.
    bool documentFormattingProvider = true;

    /// The server provides document range formatting.
    bool documentRangeFormattingProvider = true;

    /// The server provides document formatting on typing.
    DocumentOnTypeFormattingOptions documentOnTypeFormattingProvider;

    /// The server provides rename support. RenameOptions may only be
    /// specified if the client states that it supports
    /// `prepareSupport` in its initial `initialize` request.
    bool renameProvider = true;

    /// The server provides folding provider support.
    bool foldingRangeProvider = true;

    /// The server provides execute command support.
    /// executeCommandProvider?: ExecuteCommandOptions;

    /// The server provides selection range support.
    bool selectionRangeProvider = true;

    /// The server provides linked editing range support.
    bool linkedEditingRangeProvider = true;

    /// The server provides call hierarchy support.
    bool callHierarchyProvider = true;

    /// The server provides semantic tokens support.
    SemanticTokensOptions semanticTokensProvider;

    /// Whether server provides moniker support.
    bool monikerProvider = false;  // TODO: further discussion

    /// The server provides type hierarchy support.
    bool typeHierarchyProvider = true;

    /// The server provides inline values.
    bool inlineValueProvider = true;

    /// The server provides inlay hints.
    bool inlayHintProvider = true;

    /// The server has support for pull model diagnostics.
    /// TODO: diagnosticProvider?: DiagnosticOptions

    /// The server provides workspace symbol support.
    bool workspaceSymbolProvider = true;

    /// The server is interested in file notifications/requests.
    /// TODO: fileOperations?: {...}
};

struct InitializeResult {
    /// The capabilities the language server provides.
    ServerCapabilities capabilities;

    struct ServerInfo {
        /// The name of the server as defined by the server.
        std::string_view name = "clice";

        /// The server's version as defined by the server.
        std::string_view version = "0.0.1";
    };

    /// Information about the server.
    ServerInfo serverInfo;
};

/*===================================================/
/                                                    /
/=======   Text Document Synchronization   ==========/
/                                                    /
/===================================================*/

/// An item to transfer a text document from the client to the server.
struct TextDocumentItem {
    /// The text document's URI.
    std::string_view uri;

    /// The text document's language identifier.
    std::string_view languageId;

    /// he version number of this document (it will increase after each change, including undo/redo).
    int version;

    /// The content of the opened text document.
    std::string_view text;
};

/// Text documents are identified using a URI. On the protocol level, URIs are passed as strings.
struct TextDocumentIdentifier {
    /// The text document's URI.
    std::string_view uri;
};

struct VersionedTextDocumentIdentifier {
    /// The text document's URI.
    std::string_view uri;

    /// The version number of this document.
    ///
    /// The version number of a document will increase after each change,
    /// including undo/redo. The number doesn't need to be consecutive.
    int version;
};

struct DidOpenTextDocumentParams {
    /// The document that was opened.
    TextDocumentItem textDocument;
};

struct TextDocumentContentChangeEvent {
    // TODO:
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
    /// - start with the same initial content
    /// - apply the 'textDocument/didChange' notifications in the order you receive them.
    /// - apply the `TextDocumentContentChangeEvent`s in a single notification in the order you receive them.
    std::vector<TextDocumentContentChangeEvent> contentChanges;
};

struct DidCloseTextDocumentParams {
    /// The document that was closed.
    TextDocumentIdentifier textDocument;
};

struct DidSaveTextDocumentParams {
    /// The document that was saved.
    TextDocumentIdentifier textDocument;

    /// Optional the content when saved. Depends on the includeText value
    /// when the save notification was requested.
    std::optional<std::string_view> text;
};

}  // namespace clice
