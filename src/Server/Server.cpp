#include "Server/Server.h"
#include "Basic/URI.h"

namespace clice {

promise<void> Server::onInitialize(json::Value id, const proto::InitializeParams& params) {
    llvm::outs() << "onInitialize\n";
    async::write(std::move(id), json::serialize(proto::InitializeResult()));
    async::sleep(std::chrono::seconds(10));
    co_return;
}

promise<void> Server::onInitialized(const proto::InitializedParams& params) {
    llvm::outs() << "onInitialized\n";
    co_return;
}

promise<void> Server::onExit(const proto::None&) {
    llvm::outs() << "onExit\n";
    co_return;
}

promise<void> Server::onShutdown(json::Value id, const proto::None&) {
    llvm::outs() << "onShutdown\n";
    co_return;
}

promise<void> Server::onDidOpen(const proto::DidOpenTextDocumentParams& params) {
    llvm::outs() << "onDidOpen: " << params.textDocument.uri << "\n";
    auto path = URI::resolve(params.textDocument.uri);
    llvm::StringRef content = params.textDocument.text;

    co_await async::schedule_task([&]() {
        // TODO: lookup
        std::vector<const char*> args = {
            "clang++",
            "-std=c++20",
            path.c_str(),
            "-resource-dir",
            "/home/ykiko/C++/clice2/build/lib/clang/20",
        };
        Compiler compiler(path, content, args);

        auto bounds = clang::Lexer::ComputePreamble(content, {}, false);

        llvm::outs() << "Generating PCH\n";

        compiler.generatePCH("/home/ykiko/C++/clice2/build/cache/xxx.pch",
                             bounds.Size,
                             bounds.PreambleEndsAtStartOfLine);
    });

    llvm::outs() << "build PCH success\n";

    co_return;
}

promise<void> Server::onDidChange(const proto::DidChangeTextDocumentParams& document) {
    co_return;
}

promise<void> Server::onDidSave(const proto::DidSaveTextDocumentParams& document) {
    co_return;
}

promise<void> Server::onDidClose(const proto::DidCloseTextDocumentParams& document) {
    co_return;
}

promise<void> Server::onGotoDeclaration(json::Value id, const proto::DeclarationParams& params) {
    co_return;
}

promise<void> Server::onGotoDefinition(json::Value id, const proto::DefinitionParams& params) {
    co_return;
}

promise<void> Server::onGotoTypeDefinition(json::Value id,
                                           const proto::TypeDefinitionParams& params) {
    co_return;
}

promise<void> Server::onGotoImplementation(json::Value id,
                                           const proto::ImplementationParams& params) {
    co_return;
}

promise<void> Server::onFindReferences(json::Value id, const proto::ReferenceParams& params) {
    co_return;
}

promise<void> Server::onPrepareCallHierarchy(json::Value id,
                                             const proto::CallHierarchyPrepareParams& params) {
    co_return;
}

promise<void> Server::onIncomingCall(json::Value id,
                                     const proto::CallHierarchyIncomingCallsParams& params) {
    co_return;
}

promise<void> Server::onOutgoingCall(json::Value id,
                                     const proto::CallHierarchyOutgoingCallsParams& params) {
    co_return;
}

promise<void> Server::onPrepareTypeHierarchy(json::Value id,
                                             const proto::TypeHierarchyPrepareParams& params) {
    co_return;
}

promise<void> Server::onSupertypes(json::Value id,
                                   const proto::TypeHierarchySupertypesParams& params) {
    co_return;
}

promise<void> Server::onSubtypes(json::Value id, const proto::TypeHierarchySubtypesParams& params) {
    co_return;
}

promise<void> Server::onDocumentHighlight(json::Value id,
                                          const proto::DocumentHighlightParams& params) {
    co_return;
}

promise<void> Server::onDocumentLink(json::Value id, const proto::DocumentLinkParams& params) {
    co_return;
}

promise<void> Server::onHover(json::Value id, const proto::HoverParams& params) {
    co_return;
}

promise<void> Server::onCodeLens(json::Value id, const proto::CodeLensParams& params) {
    co_return;
}

promise<void> Server::onFoldingRange(json::Value id, const proto::FoldingRangeParams& params) {
    co_return;
}

promise<void> Server::onDocumentSymbol(json::Value id, const proto::DocumentSymbolParams& params) {
    co_return;
}

promise<void> Server::onSemanticTokens(json::Value id, const proto::SemanticTokensParams& params) {
    co_return;
}

promise<void> Server::onInlayHint(json::Value id, const proto::InlayHintParams& params) {
    co_return;
}

promise<void> Server::onCodeCompletion(json::Value id, const proto::CompletionParams& params) {
    co_return;
}

promise<void> Server::onSignatureHelp(json::Value id, const proto::SignatureHelpParams& params) {
    co_return;
}

promise<void> Server::onCodeAction(json::Value id, const proto::CodeActionParams& params) {
    co_return;
}

promise<void> Server::onFormatting(json::Value id, const proto::DocumentFormattingParams& params) {
    co_return;
}

promise<void> Server::onRangeFormatting(json::Value id,
                                        const proto::DocumentRangeFormattingParams& params) {
    co_return;
}

promise<void> Server::dispatch(json::Value value) {
    assert(value.kind() == json::Value::Object);
    auto object = value.getAsObject();
    assert(object && "value is not an object");
    if(auto method = object->get("method")) {
        auto name = *method->getAsString();
        auto params = object->get("params");
        if(auto id = object->get("id")) {
            if(auto iter = requests.find(name); iter != requests.end()) {
                co_await iter->second(std::move(*id),
                                      params ? std::move(*params) : json::Value(nullptr));
            } else {
                llvm::errs() << "Unknown request: " << name << "\n";
            }
        } else {
            if(auto iter = notifications.find(name); iter != notifications.end()) {
                co_await iter->second(params ? std::move(*params) : json::Value(nullptr));
            } else {
                llvm::errs() << "Unknown notification: " << name << "\n";
            }
        }
    }

    co_return;
}

}  // namespace clice
