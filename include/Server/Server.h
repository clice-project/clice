#pragma once

#include "Async.h"
#include "Protocol.h"

namespace clice {

class Server {
public:
    Server();

    int run(int argc, const char** argv);

private:
    using onRequest = llvm::unique_function<async::Task<>(json::Value, json::Value)>;
    using onNotification = llvm::unique_function<async::Task<>(json::Value)>;

    template <typename Param>
    void addMethod(llvm::StringRef name,
                   async::Task<> (Server::*method)(json::Value, const Param&)) {
        requests.try_emplace(name,
                             [this, method](json::Value id, json::Value value) -> async::Task<> {
                                 co_await (this->*method)(std::move(id),
                                                          json::deserialize<Param>(value));
                             });
    }

    template <typename Param>
    void addMethod(llvm::StringRef name, async::Task<> (Server::*method)(const Param&)) {
        notifications.try_emplace(name, [this, method](json::Value value) -> async::Task<> {
            co_await (this->*method)(json::deserialize<Param>(value));
        });
    }

    llvm::StringMap<onRequest> requests;
    llvm::StringMap<onNotification> notifications;

private:
    /// ============================================================================
    ///                            Lifecycle Message
    /// ============================================================================

    async::Task<> onInitialize(json::Value id, const proto::InitializeParams& params);

    async::Task<> onInitialized(const proto::InitializedParams& params);

    async::Task<> onShutdown(json::Value id, const proto::None&);

    async::Task<> onExit(const proto::None&);

    /// ============================================================================
    ///                         Document Synchronization
    /// ============================================================================

    async::Task<> onDidOpen(const proto::DidOpenTextDocumentParams& document);

    async::Task<> onDidChange(const proto::DidChangeTextDocumentParams& document);

    async::Task<> onDidSave(const proto::DidSaveTextDocumentParams& document);

    async::Task<> onDidClose(const proto::DidCloseTextDocumentParams& document);

    /// ============================================================================
    ///                             Language Features
    /// ============================================================================

    async::Task<> onGotoDeclaration(json::Value id, const proto::DeclarationParams& params);

    async::Task<> onGotoDefinition(json::Value id, const proto::DefinitionParams& params);

    async::Task<> onGotoTypeDefinition(json::Value id, const proto::TypeDefinitionParams& params);

    async::Task<> onGotoImplementation(json::Value id, const proto::ImplementationParams& params);

    async::Task<> onFindReferences(json::Value id, const proto::ReferenceParams& params);

    async::Task<> onPrepareCallHierarchy(json::Value id,
                                         const proto::CallHierarchyPrepareParams& params);

    async::Task<> onIncomingCall(json::Value id,
                                 const proto::CallHierarchyIncomingCallsParams& params);

    async::Task<> onOutgoingCall(json::Value id,
                                 const proto::CallHierarchyOutgoingCallsParams& params);

    async::Task<> onPrepareTypeHierarchy(json::Value id,
                                         const proto::TypeHierarchyPrepareParams& params);

    async::Task<> onSupertypes(json::Value id, const proto::TypeHierarchySupertypesParams& params);

    async::Task<> onSubtypes(json::Value id, const proto::TypeHierarchySubtypesParams& params);

    async::Task<> onDocumentHighlight(json::Value id, const proto::DocumentHighlightParams& params);

    async::Task<> onDocumentLink(json::Value id, const proto::DocumentLinkParams& params);

    async::Task<> onHover(json::Value id, const proto::HoverParams& params);

    async::Task<> onCodeLens(json::Value id, const proto::CodeLensParams& params);

    async::Task<> onFoldingRange(json::Value id, const proto::FoldingRangeParams& params);

    async::Task<> onDocumentSymbol(json::Value id, const proto::DocumentSymbolParams& params);

    async::Task<> onSemanticTokens(json::Value id, const proto::SemanticTokensParams& params);

    async::Task<> onInlayHint(json::Value id, const proto::InlayHintParams& params);

    async::Task<> onCodeCompletion(json::Value id, const proto::CompletionParams& params);

    async::Task<> onSignatureHelp(json::Value id, const proto::SignatureHelpParams& params);

    async::Task<> onCodeAction(json::Value id, const proto::CodeActionParams& params);

    async::Task<> onFormatting(json::Value id, const proto::DocumentFormattingParams& params);

    async::Task<> onRangeFormatting(json::Value id,
                                    const proto::DocumentRangeFormattingParams& params);

    /// ============================================================================
    ///                             Workspace Features
    /// ============================================================================

    async::Task<> onDidChangeWatchedFiles(const proto::DidChangeWatchedFilesParams& params);

    /// ============================================================================
    ///                                 Extension
    /// ============================================================================

    async::Task<> onContextCurrent(const proto::TextDocumentIdentifier& params);

    async::Task<> onContextAll(const proto::TextDocumentIdentifier& params);

    async::Task<> onContextSwitch(const proto::TextDocumentIdentifier& params);
};

}  // namespace clice
