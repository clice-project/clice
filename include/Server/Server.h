#pragma once

#include "Async.h"
#include "Config.h"
#include "Basic/Document.h"
#include "Compiler/Compiler.h"
#include "Support/JSON.h"

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

#include "llvm/ADT/FunctionExtras.h"

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

struct ClientCapabilities {};

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
    // SemanticTokensOptions semanticTokensProvider;
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

namespace clice {

class CacheManager {
public:
    promise<void> buildPCH(std::string file, std::string content);

private:
    std::string outDir;
};

class LSPServer {
public:
    LSPServer() {
        addMethod("initialize", &LSPServer::onInitialize);
        addMethod("initialized", &LSPServer::onInitialized);
        addMethod("shutdown", &LSPServer::onShutdown);
        addMethod("exit", &LSPServer::onExit);

        addMethod("textDocument/didOpen", &LSPServer::onDidOpen);
        addMethod("textDocument/didChange", &LSPServer::onDidChange);
        addMethod("textDocument/didSave", &LSPServer::onDidSave);
        addMethod("textDocument/didClose", &LSPServer::onDidClose);

        addMethod("textDocument/declaration", &LSPServer::onGotoDeclaration);
        addMethod("textDocument/definition", &LSPServer::onGotoDefinition);
        addMethod("textDocument/typeDefinition", &LSPServer::onGotoTypeDefinition);
        addMethod("textDocument/implementation", &LSPServer::onGotoImplementation);
        addMethod("textDocument/references", &LSPServer::onFindReferences);
        addMethod("textDocument/callHierarchy/prepare", &LSPServer::onPrepareCallHierarchy);
        addMethod("textDocument/callHierarchy/incomingCalls", &LSPServer::onIncomingCall);
        addMethod("textDocument/callHierarchy/outgoingCalls", &LSPServer::onOutgoingCall);
        addMethod("textDocument/typeHierarchy/prepare", &LSPServer::onPrepareTypeHierarchy);
        addMethod("textDocument/typeHierarchy/supertypes", &LSPServer::onSupertypes);
        addMethod("textDocument/typeHierarchy/subtypes", &LSPServer::onSubtypes);
        addMethod("textDocument/documentHighlight", &LSPServer::onDocumentHighlight);
        addMethod("textDocument/documentLink", &LSPServer::onDocumentLink);
        addMethod("textDocument/hover", &LSPServer::onHover);
        addMethod("textDocument/codeLens", &LSPServer::onCodeLens);
        addMethod("textDocument/foldingRange", &LSPServer::onFoldingRange);
        addMethod("textDocument/documentSymbol", &LSPServer::onDocumentSymbol);
        addMethod("textDocument/semanticTokens", &LSPServer::onSemanticTokens);
        addMethod("textDocument/inlayHint", &LSPServer::onInlayHint);
        addMethod("textDocument/completion", &LSPServer::onCodeCompletion);
        addMethod("textDocument/signatureHelp", &LSPServer::onSignatureHelp);
        addMethod("textDocument/codeAction", &LSPServer::onCodeAction);
        addMethod("textDocument/formatting", &LSPServer::onFormatting);
        addMethod("textDocument/rangeFormatting", &LSPServer::onRangeFormatting);
    }

    promise<void> dispatch(json::Value value, Writer& writer);

private:
    using onRequest = llvm::unique_function<promise<void>(json::Value, json::Value)>;
    using onNotification = llvm::unique_function<promise<void>(json::Value)>;

    template <typename Param>
    void addMethod(llvm::StringRef name,
                   promise<void> (LSPServer::*method)(json::Value, const Param&)) {
        requests.try_emplace(name,
                             [this, method](json::Value id, json::Value value) -> promise<void> {
                                 co_await (this->*method)(std::move(id),
                                                          json::deserialize<Param>(value));
                             });
    }

    template <typename Param>
    void addMethod(llvm::StringRef name, promise<void> (LSPServer::*method)(const Param&)) {
        notifications.try_emplace(name, [this, method](json::Value value) -> promise<void> {
            co_await (this->*method)(json::deserialize<Param>(value));
        });
    }

private:
    /// ============================================================================
    ///                            Lifestyle Message
    /// ============================================================================

    promise<void> onInitialize(json::Value id, const proto::InitializeParams& params);

    promise<void> onInitialized(const proto::InitializedParams& params);

    promise<void> onShutdown(json::Value id, const proto::None&);

    promise<void> onExit(const proto::None&);

    /// ============================================================================
    ///                         Document Synchronization
    /// ============================================================================

    promise<void> onDidOpen(const proto::DidOpenTextDocumentParams& document);

    promise<void> onDidChange(const proto::DidChangeTextDocumentParams& document);

    promise<void> onDidSave(const proto::DidSaveTextDocumentParams& document);

    promise<void> onDidClose(const proto::DidCloseTextDocumentParams& document);

    /// ============================================================================
    ///                             Language Features
    /// ============================================================================

    promise<void> onGotoDeclaration(json::Value id, const proto::DeclarationParams& params);

    promise<void> onGotoDefinition(json::Value id, const proto::DefinitionParams& params);

    promise<void> onGotoTypeDefinition(json::Value id, const proto::TypeDefinitionParams& params);

    promise<void> onGotoImplementation(json::Value id, const proto::ImplementationParams& params);

    promise<void> onFindReferences(json::Value id, const proto::ReferenceParams& params);

    promise<void> onPrepareCallHierarchy(json::Value id,
                                         const proto::CallHierarchyPrepareParams& params);

    promise<void> onIncomingCall(json::Value id,
                                 const proto::CallHierarchyIncomingCallsParams& params);

    promise<void> onOutgoingCall(json::Value id,
                                 const proto::CallHierarchyOutgoingCallsParams& params);

    promise<void> onPrepareTypeHierarchy(json::Value id,
                                         const proto::TypeHierarchyPrepareParams& params);

    promise<void> onSupertypes(json::Value id, const proto::TypeHierarchySupertypesParams& params);

    promise<void> onSubtypes(json::Value id, const proto::TypeHierarchySubtypesParams& params);

    promise<void> onDocumentHighlight(json::Value id, const proto::DocumentHighlightParams& params);

    promise<void> onDocumentLink(json::Value id, const proto::DocumentLinkParams& params);

    promise<void> onHover(json::Value id, const proto::HoverParams& params);

    promise<void> onCodeLens(json::Value id, const proto::CodeLensParams& params);

    promise<void> onFoldingRange(json::Value id, const proto::FoldingRangeParams& params);

    promise<void> onDocumentSymbol(json::Value id, const proto::DocumentSymbolParams& params);

    promise<void> onSemanticTokens(json::Value id, const proto::SemanticTokensParams& params);

    promise<void> onInlayHint(json::Value id, const proto::InlayHintParams& params);

    promise<void> onCodeCompletion(json::Value id, const proto::CompletionParams& params);

    promise<void> onSignatureHelp(json::Value id, const proto::SignatureHelpParams& params);

    promise<void> onCodeAction(json::Value id, const proto::CodeActionParams& params);

    promise<void> onFormatting(json::Value id, const proto::DocumentFormattingParams& params);

    promise<void> onRangeFormatting(json::Value id,
                                    const proto::DocumentRangeFormattingParams& params);

private:
    Writer* writer;
    llvm::StringMap<onRequest> requests;
    llvm::StringMap<onNotification> notifications;
};

}  // namespace clice
