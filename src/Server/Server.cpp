#include "Server/Server.h"

namespace clice {

Server::Server() {
    addMethod("initialize", &Server::onInitialize);
    addMethod("initialized", &Server::onInitialized);
    addMethod("shutdown", &Server::onShutdown);
    addMethod("exit", &Server::onExit);

    addMethod("textDocument/didOpen", &Server::onDidOpen);
    addMethod("textDocument/didChange", &Server::onDidChange);
    addMethod("textDocument/didSave", &Server::onDidSave);
    addMethod("textDocument/didClose", &Server::onDidClose);

    addMethod("textDocument/declaration", &Server::onGotoDeclaration);
    addMethod("textDocument/definition", &Server::onGotoDefinition);
    addMethod("textDocument/typeDefinition", &Server::onGotoTypeDefinition);
    addMethod("textDocument/implementation", &Server::onGotoImplementation);
    addMethod("textDocument/references", &Server::onFindReferences);
    addMethod("textDocument/callHierarchy/prepare", &Server::onPrepareCallHierarchy);
    addMethod("textDocument/callHierarchy/incomingCalls", &Server::onIncomingCall);
    addMethod("textDocument/callHierarchy/outgoingCalls", &Server::onOutgoingCall);
    addMethod("textDocument/typeHierarchy/prepare", &Server::onPrepareTypeHierarchy);
    addMethod("textDocument/typeHierarchy/supertypes", &Server::onSupertypes);
    addMethod("textDocument/typeHierarchy/subtypes", &Server::onSubtypes);
    addMethod("textDocument/documentHighlight", &Server::onDocumentHighlight);
    addMethod("textDocument/documentLink", &Server::onDocumentLink);
    addMethod("textDocument/hover", &Server::onHover);
    addMethod("textDocument/codeLens", &Server::onCodeLens);
    addMethod("textDocument/foldingRange", &Server::onFoldingRange);
    addMethod("textDocument/documentSymbol", &Server::onDocumentSymbol);
    addMethod("textDocument/semanticTokens/full", &Server::onSemanticTokens);
    addMethod("textDocument/inlayHint", &Server::onInlayHint);
    addMethod("textDocument/completion", &Server::onCodeCompletion);
    addMethod("textDocument/signatureHelp", &Server::onSignatureHelp);
    addMethod("textDocument/codeAction", &Server::onCodeAction);
    addMethod("textDocument/formatting", &Server::onFormatting);
    addMethod("textDocument/rangeFormatting", &Server::onRangeFormatting);
}

void Server::run(int argc, const char** argv) {
    auto dispatch = [this](json::Value value) -> async::promise<void> {
        assert(value.kind() == json::Value::Object);
        auto object = value.getAsObject();
        assert(object && "value is not an object");
        if(auto method = object->get("method")) {
            auto name = *method->getAsString();
            auto params = object->get("params");
            if(auto id = object->get("id")) {
                if(auto iter = requests.find(name); iter != requests.end()) {
                    log::info("Request: {0}", name.str());
                    co_await iter->second(std::move(*id),
                                          params ? std::move(*params) : json::Value(nullptr));
                } else {
                    log::warn("Unknown request: {0}", name.str());
                }
            } else {
                if(auto iter = notifications.find(name); iter != notifications.end()) {
                    log::info("Notification: {0}", name.str());
                    co_await iter->second(params ? std::move(*params) : json::Value(nullptr));
                } else {
                    log::warn("Unknown notification: {0}", name.str());
                }
            }
        }

        co_return;
    };

    async::start_server(dispatch, "127.0.0.1", 50051);
}

async::promise<void> Server::onInitialize(json::Value id, const proto::InitializeParams& params) {
    auto workplace = URI::resolve(params.workspaceFolders[0].uri);
    config::init(workplace);
    auto result = json::serialize(proto::InitializeResult());
    auto capabilities = result.getAsObject()->get("capabilities")->getAsObject();
    /// FIXME:
    capabilities->try_emplace("completionProvider", feature::capability(json::Value(nullptr)));
    async::write(std::move(id), std::move(result));
    co_return;
}

async::promise<void> Server::onInitialized(const proto::InitializedParams& params) {
    co_return;
}

async::promise<void> Server::onExit(const proto::None&) {
    co_return;
}

async::promise<void> Server::onShutdown(json::Value id, const proto::None&) {
    co_return;
}

async::promise<void> Server::onDidOpen(const proto::DidOpenTextDocumentParams& params) {
    auto path = URI::resolve(params.textDocument.uri);
    llvm::StringRef content = params.textDocument.text;

    co_await scheduler.add(path, content);
}

async::promise<void> Server::onDidChange(const proto::DidChangeTextDocumentParams& document) {
    auto path = URI::resolve(document.textDocument.uri);
    llvm::StringRef content = document.contentChanges[0].text;
    co_await scheduler.update(path, content);
}

async::promise<void> Server::onDidSave(const proto::DidSaveTextDocumentParams& document) {
    auto path = URI::resolve(document.textDocument.uri);
    co_await scheduler.save(path);
    co_return;
}

async::promise<void> Server::onDidClose(const proto::DidCloseTextDocumentParams& document) {
    auto path = URI::resolve(document.textDocument.uri);
    co_await scheduler.close(path);
    co_return;
}

async::promise<void> Server::onGotoDeclaration(json::Value id,
                                               const proto::DeclarationParams& params) {
    co_return;
}

async::promise<void> Server::onGotoDefinition(json::Value id,
                                              const proto::DefinitionParams& params) {
    co_return;
}

async::promise<void> Server::onGotoTypeDefinition(json::Value id,
                                                  const proto::TypeDefinitionParams& params) {
    co_return;
}

async::promise<void> Server::onGotoImplementation(json::Value id,
                                                  const proto::ImplementationParams& params) {
    co_return;
}

async::promise<void> Server::onFindReferences(json::Value id,
                                              const proto::ReferenceParams& params) {
    co_return;
}

async::promise<void>
    Server::onPrepareCallHierarchy(json::Value id,
                                   const proto::CallHierarchyPrepareParams& params) {
    co_return;
}

async::promise<void> Server::onIncomingCall(json::Value id,
                                            const proto::CallHierarchyIncomingCallsParams& params) {
    co_return;
}

async::promise<void> Server::onOutgoingCall(json::Value id,
                                            const proto::CallHierarchyOutgoingCallsParams& params) {
    co_return;
}

async::promise<void>
    Server::onPrepareTypeHierarchy(json::Value id,
                                   const proto::TypeHierarchyPrepareParams& params) {
    co_return;
}

async::promise<void> Server::onSupertypes(json::Value id,
                                          const proto::TypeHierarchySupertypesParams& params) {
    co_return;
}

async::promise<void> Server::onSubtypes(json::Value id,
                                        const proto::TypeHierarchySubtypesParams& params) {
    co_return;
}

async::promise<void> Server::onDocumentHighlight(json::Value id,
                                                 const proto::DocumentHighlightParams& params) {
    co_return;
}

async::promise<void> Server::onDocumentLink(json::Value id,
                                            const proto::DocumentLinkParams& params) {
    co_return;
}

async::promise<void> Server::onHover(json::Value id, const proto::HoverParams& params) {
    co_return;
}

async::promise<void> Server::onCodeLens(json::Value id, const proto::CodeLensParams& params) {
    co_return;
}

async::promise<void> Server::onFoldingRange(json::Value id,
                                            const proto::FoldingRangeParams& params) {
    co_return;
}

async::promise<void> Server::onDocumentSymbol(json::Value id,
                                              const proto::DocumentSymbolParams& params) {
    co_return;
}

async::promise<void> Server::onSemanticTokens(json::Value id,
                                              const proto::SemanticTokensParams& params) {
    auto path = URI::resolve(params.textDocument.uri);
    auto tokens = co_await scheduler.schedule(path, [&](ASTInfo& compiler) {
        return feature::semanticTokens(compiler, "");
    });
    async::write(std::move(id), json::serialize(tokens));
    co_return;
}

async::promise<void> Server::onInlayHint(json::Value id, const proto::InlayHintParams& params) {
    co_return;
}

async::promise<void> Server::onCodeCompletion(json::Value id,
                                              const proto::CompletionParams& params) {
    auto path = URI::resolve(params.textDocument.uri);
    auto result = co_await scheduler.codeComplete(path,
                                                  params.position.line + 1,
                                                  params.position.character + 1);
    async::write(std::move(id), json::serialize(result));
    co_return;
}

async::promise<void> Server::onSignatureHelp(json::Value id,
                                             const proto::SignatureHelpParams& params) {
    co_return;
}

async::promise<void> Server::onCodeAction(json::Value id, const proto::CodeActionParams& params) {
    co_return;
}

async::promise<void> Server::onFormatting(json::Value id,
                                          const proto::DocumentFormattingParams& params) {
    co_return;
}

async::promise<void> Server::onRangeFormatting(json::Value id,
                                               const proto::DocumentRangeFormattingParams& params) {
    co_return;
}

}  // namespace clice
