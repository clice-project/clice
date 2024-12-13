#pragma once

#include "Compiler/Compiler.h"
#include "Server/Async.h"
#include "Server/Config.h"
#include "Server/Logger.h"
#include "Server/Scheduler.h"
#include "Server/Protocol.h"
#include "Support/Support.h"

namespace clice {

class Server {
public:
    Server();

    int run(int argc, const char** argv);

private:
    using onRequest = llvm::unique_function<async::promise<void>(json::Value, json::Value)>;
    using onNotification = llvm::unique_function<async::promise<void>(json::Value)>;

    template <typename Param>
    void addMethod(llvm::StringRef name,
                   async::promise<void> (Server::*method)(json::Value, const Param&)) {
        requests.try_emplace(
            name,
            [this, method](json::Value id, json::Value value) -> async::promise<void> {
                co_await (this->*method)(std::move(id), json::deserialize<Param>(value));
            });
    }

    template <typename Param>
    void addMethod(llvm::StringRef name, async::promise<void> (Server::*method)(const Param&)) {
        notifications.try_emplace(name, [this, method](json::Value value) -> async::promise<void> {
            co_await (this->*method)(json::deserialize<Param>(value));
        });
    }

private:
    /// ============================================================================
    ///                            Lifestyle Message
    /// ============================================================================

    async::promise<void> onInitialize(json::Value id, const proto::InitializeParams& params);

    async::promise<void> onInitialized(const proto::InitializedParams& params);

    async::promise<void> onShutdown(json::Value id, const proto::None&);

    async::promise<void> onExit(const proto::None&);

    /// ============================================================================
    ///                         Document Synchronization
    /// ============================================================================

    async::promise<void> onDidOpen(const proto::DidOpenTextDocumentParams& document);

    async::promise<void> onDidChange(const proto::DidChangeTextDocumentParams& document);

    async::promise<void> onDidSave(const proto::DidSaveTextDocumentParams& document);

    async::promise<void> onDidClose(const proto::DidCloseTextDocumentParams& document);

    /// ============================================================================
    ///                             Language Features
    /// ============================================================================

    async::promise<void> onGotoDeclaration(json::Value id, const proto::DeclarationParams& params);

    async::promise<void> onGotoDefinition(json::Value id, const proto::DefinitionParams& params);

    async::promise<void> onGotoTypeDefinition(json::Value id,
                                              const proto::TypeDefinitionParams& params);

    async::promise<void> onGotoImplementation(json::Value id,
                                              const proto::ImplementationParams& params);

    async::promise<void> onFindReferences(json::Value id, const proto::ReferenceParams& params);

    async::promise<void> onPrepareCallHierarchy(json::Value id,
                                                const proto::CallHierarchyPrepareParams& params);

    async::promise<void> onIncomingCall(json::Value id,
                                        const proto::CallHierarchyIncomingCallsParams& params);

    async::promise<void> onOutgoingCall(json::Value id,
                                        const proto::CallHierarchyOutgoingCallsParams& params);

    async::promise<void> onPrepareTypeHierarchy(json::Value id,
                                                const proto::TypeHierarchyPrepareParams& params);

    async::promise<void> onSupertypes(json::Value id,
                                      const proto::TypeHierarchySupertypesParams& params);

    async::promise<void> onSubtypes(json::Value id,
                                    const proto::TypeHierarchySubtypesParams& params);

    async::promise<void> onDocumentHighlight(json::Value id,
                                             const proto::DocumentHighlightParams& params);

    async::promise<void> onDocumentLink(json::Value id, const proto::DocumentLinkParams& params);

    async::promise<void> onHover(json::Value id, const proto::HoverParams& params);

    async::promise<void> onCodeLens(json::Value id, const proto::CodeLensParams& params);

    async::promise<void> onFoldingRange(json::Value id, const proto::FoldingRangeParams& params);

    async::promise<void> onDocumentSymbol(json::Value id,
                                          const proto::DocumentSymbolParams& params);

    async::promise<void> onSemanticTokens(json::Value id,
                                          const proto::SemanticTokensParams& params);

    async::promise<void> onInlayHint(json::Value id, const proto::InlayHintParams& params);

    async::promise<void> onCodeCompletion(json::Value id, const proto::CompletionParams& params);

    async::promise<void> onSignatureHelp(json::Value id, const proto::SignatureHelpParams& params);

    async::promise<void> onCodeAction(json::Value id, const proto::CodeActionParams& params);

    async::promise<void> onFormatting(json::Value id,
                                      const proto::DocumentFormattingParams& params);

    async::promise<void> onRangeFormatting(json::Value id,
                                           const proto::DocumentRangeFormattingParams& params);

    /// ============================================================================
    ///                             Workspace Features
    /// ============================================================================

    async::promise<void> onDidChangeWatchedFiles(const proto::DidChangeWatchedFilesParams& params);

    /// ============================================================================
    ///                                 Extension
    /// ============================================================================

    async::promise<void> onContextCurrent(const proto::TextDocumentIdentifier& params);

    async::promise<void> onContextAll(const proto::TextDocumentIdentifier& params);

    async::promise<void> onContextSwitch(const proto::TextDocumentIdentifier& params);

private:
    Scheduler scheduler;
    llvm::StringMap<onRequest> requests;
    llvm::StringMap<onNotification> notifications;
};

}  // namespace clice
