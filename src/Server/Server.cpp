#include "Server/Logger.h"
#include "Server/Server.h"

namespace clice {

Server::Server() : indexer({},database), scheduler(database, {}) {
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

    addMethod("workspace/didChangeWatchedFiles", &Server::onDidChangeWatchedFiles);

    addMethod("index/current", &Server::onIndexCurrent);
    addMethod("index/all", &Server::onIndexAll);
    addMethod("context/current", &Server::onContextCurrent);
    addMethod("context/switch", &Server::onContextSwitch);
    addMethod("context/all", &Server::onContextAll);
}

async::Task<> Server::onReceive(json::Value value) {
    assert(value.kind() == json::Value::Object);
    auto object = value.getAsObject();
    assert(object && "value is not an object");
    if(auto method = object->get("method")) {
        auto name = *method->getAsString();
        auto params = object->get("params");
        if(auto id = object->get("id")) {
            if(auto iter = requests.find(name); iter != requests.end()) {
                /// auto tracer = Tracer();
                log::info("Receive request: {0}", name);
                co_await iter->second(std::move(*id),
                                      params ? std::move(*params) : json::Value(nullptr));
                log::info("Request {0} is done, elapsed {1}", name, 0);

            } else {
                log::warn("Unknown request: {0}", name);
            }
        } else {
            if(auto iter = notifications.find(name); iter != notifications.end()) {
                log::info("Notification: {0}", name);
                co_await iter->second(params ? std::move(*params) : json::Value(nullptr));
            } else {
                log::warn("Unknown notification: {0}", name);
            }
        }
    }
    co_return;
}

async::Task<> Server::request(llvm::StringRef method, json::Value params) {
    co_await async::write(json::Object{
        {"jsonrpc", "2.0"            },
        {"id",      id += 1          },
        {"method",  method           },
        {"params",  std::move(params)},
    });
}

async::Task<> Server::notify(llvm::StringRef method, json::Value params) {
    co_await async::write(json::Object{
        {"jsonrpc", "2.0"            },
        {"method",  method           },
        {"params",  std::move(params)},
    });
}

async::Task<> Server::response(json::Value id, json::Value result) {
    co_await async::write(json::Object{
        {"jsonrpc", "2.0"            },
        {"id",      id               },
        {"result",  std::move(result)},
    });
}

async::Task<> Server::registerCapacity(llvm::StringRef id,
                                       llvm::StringRef method,
                                       json::Value registerOptions) {
    co_await request("client/registerCapability",
                     json::Object{
                         {"registrations",
                          json::Array{json::Object{
                              {"id", id},
                              {"method", method},
                              {"registerOptions", std::move(registerOptions)},
                          }}},
    });
}

}  // namespace clice
