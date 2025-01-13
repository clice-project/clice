#include "Basic/SourceConverter.h"
#include "Server/Server.h"

namespace clice {

async::Task<> Server::onGotoDeclaration(json::Value id, const proto::DeclarationParams& params) {
    co_return;
}

async::Task<> Server::onGotoDefinition(json::Value id, const proto::DefinitionParams& params) {
    co_return;
}

async::Task<> Server::onGotoTypeDefinition(json::Value id,
                                           const proto::TypeDefinitionParams& params) {
    co_return;
}

async::Task<> Server::onGotoImplementation(json::Value id,
                                           const proto::ImplementationParams& params) {
    co_return;
}

async::Task<> Server::onFindReferences(json::Value id, const proto::ReferenceParams& params) {
    co_return;
}

async::Task<> Server::onPrepareCallHierarchy(json::Value id,
                                             const proto::CallHierarchyPrepareParams& params) {
    co_return;
}

async::Task<> Server::onIncomingCall(json::Value id,
                                     const proto::CallHierarchyIncomingCallsParams& params) {
    co_return;
}

async::Task<> Server::onOutgoingCall(json::Value id,
                                     const proto::CallHierarchyOutgoingCallsParams& params) {
    co_return;
}

async::Task<> Server::onPrepareTypeHierarchy(json::Value id,
                                             const proto::TypeHierarchyPrepareParams& params) {
    co_return;
}

async::Task<> Server::onSupertypes(json::Value id,
                                   const proto::TypeHierarchySupertypesParams& params) {
    co_return;
}

async::Task<> Server::onSubtypes(json::Value id, const proto::TypeHierarchySubtypesParams& params) {
    co_return;
}

async::Task<> Server::onDocumentHighlight(json::Value id,
                                          const proto::DocumentHighlightParams& params) {
    co_return;
}

async::Task<> Server::onDocumentLink(json::Value id, const proto::DocumentLinkParams& params) {
    co_return;
}

async::Task<> Server::onHover(json::Value id, const proto::HoverParams& params) {
    co_return;
}

async::Task<> Server::onCodeLens(json::Value id, const proto::CodeLensParams& params) {
    co_return;
}

async::Task<> Server::onFoldingRange(json::Value id, const proto::FoldingRangeParams& params) {
    co_return;
}

async::Task<> Server::onDocumentSymbol(json::Value id, const proto::DocumentSymbolParams& params) {
    co_return;
}

async::Task<> Server::onSemanticTokens(json::Value id, const proto::SemanticTokensParams& params) {
    co_return;
}

async::Task<> Server::onInlayHint(json::Value id, const proto::InlayHintParams& params) {
    co_return;
}

async::Task<> Server::onCodeCompletion(json::Value id, const proto::CompletionParams& params) {
    // auto path = URI::resolve(params.textDocument.uri);
    // async::response(std::move(id), json::serialize(result));
    co_return;
}

async::Task<> Server::onSignatureHelp(json::Value id, const proto::SignatureHelpParams& params) {
    co_return;
}

async::Task<> Server::onCodeAction(json::Value id, const proto::CodeActionParams& params) {
    co_return;
}

async::Task<> Server::onFormatting(json::Value id, const proto::DocumentFormattingParams& params) {
    co_return;
}

async::Task<> Server::onRangeFormatting(json::Value id,
                                        const proto::DocumentRangeFormattingParams& params) {
    co_return;
}

}  // namespace clice
