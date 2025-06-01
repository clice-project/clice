#include "Support/Logger.h"
#include "Server/Server.h"

namespace clice {

async::Task<> Server::request(llvm::StringRef method, json::Value params) {
    co_await async::net::write(json::Object{
        {"jsonrpc", "2.0"            },
        {"id",      id += 1          },
        {"method",  method           },
        {"params",  std::move(params)},
    });
}

async::Task<> Server::notify(llvm::StringRef method, json::Value params) {
    co_await async::net::write(json::Object{
        {"jsonrpc", "2.0"            },
        {"method",  method           },
        {"params",  std::move(params)},
    });
}

async::Task<> Server::response(json::Value id, json::Value result) {
    co_await async::net::write(json::Object{
        {"jsonrpc", "2.0"            },
        {"id",      std::move(id)    },
        {"result",  std::move(result)},
    });
}

async::Task<> Server::response(json::Value id, proto::ErrorCodes code, llvm::StringRef message) {
    json::Object error{
        {"code",    static_cast<int>(code)},
        {"message", message               },
    };

    co_await async::net::write(json::Object{
        {"jsonrpc", "2.0"           },
        {"id",      std::move(id)   },
        {"error",   std::move(error)},
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

Server::Server() : scheduler(indexer, converter, database) {
    onRequests.try_emplace("initialize", &Server::onInitialize);
    onRequests.try_emplace("textDocument/semanticTokens/full", &Server::onSemanticToken);
    onRequests.try_emplace("textDocument/completion", &Server::onCodeCompletion);
    onNotifications.try_emplace("textDocument/didOpen", &Server::onDidOpen);
    onNotifications.try_emplace("textDocument/didChange", &Server::onDidChange);
    onNotifications.try_emplace("textDocument/didSave", &Server::onDidSave);
    onNotifications.try_emplace("textDocument/didClose", &Server::onDidClose);
}

async::Task<> Server::onReceive(json::Value value) {
    auto object = value.getAsObject();
    if(!object) [[unlikely]] {
        log::fatal("Invalid LSP message, not an object: {}", value);
    }

    /// If the json object has an `id`, it's a request,
    /// which needs a response. Otherwise, it's a notification.
    auto id = object->get("id");

    llvm::StringRef method;
    if(auto result = object->getString("method")) {
        method = *result;
    } else [[unlikely]] {
        log::warn("Invalid LSP message, method not found: {}", value);
        if(id) {
            co_await response(std::move(*id),
                              proto::ErrorCodes::InvalidRequest,
                              "Method not found");
        }
        co_return;
    }

    json::Value params = json::Value(nullptr);
    if(auto result = object->get("params")) {
        params = std::move(*result);
    }

    /// Handle request and notification separately.
    /// TODO: Record the time of handling request and notification.
    if(id) {
        log::info("Handling request: {}", method);
        if(auto iter = onRequests.find(method); iter != onRequests.end()) {
            auto result = co_await (this->*(iter->second))(std::move(params));
            co_await response(std::move(*id), std::move(result));
        }
        log::info("Handled request: {}", method);
    } else {
        log::info("Handling notification: {}", method);
        if(auto iter = onNotifications.find(method); iter != onNotifications.end()) {
            co_await (this->*(iter->second))(std::move(params));
        }
        log::info("Handled notification: {}", method);
    }

    co_return;
}

async::Task<json::Value> Server::onInitialize(json::Value value) {
    auto result = converter.initialize(std::move(value));
    config::init(converter.workspace());

    for(auto& dir: config::server.compile_commands_dirs) {
        database.updateCommands(dir + "/compile_commands.json");
    }

    co_return result;
}

async::Task<> Server::onDidOpen(json::Value value) {
    struct DidOpenTextDocumentParams {
        proto::TextDocumentItem textDocument;
    };

    auto params = json::deserialize<DidOpenTextDocumentParams>(value);
    auto path = converter.convert(params.textDocument.uri);
    scheduler.addDocument(std::move(path), std::move(params.textDocument.text));
    co_return;
}

async::Task<> Server::onDidChange(json::Value value) {
    struct DidChangeTextDocumentParams {
        proto::VersionedTextDocumentIdentifier textDocument;

        struct TextDocumentContentChangeEvent {
            std::string text;
        };

        std::vector<TextDocumentContentChangeEvent> contentChanges;
    };

    auto params = json::deserialize<DidChangeTextDocumentParams>(value);
    auto path = converter.convert(params.textDocument.uri);
    scheduler.addDocument(std::move(path), std::move(params.contentChanges[0].text));

    co_return;
}

async::Task<> Server::onDidSave(json::Value value) {
    co_return;
}

async::Task<> Server::onDidClose(json::Value value) {
    co_return;
}

async::Task<json::Value> Server::onSemanticToken(json::Value value) {
    struct SemanticTokensParams {
        proto::TextDocumentIdentifier textDocument;
    };

    auto params = json::deserialize<SemanticTokensParams>(value);
    auto path = converter.convert(params.textDocument.uri);
    co_return co_await scheduler.semanticToken(std::move(path));
}

async::Task<json::Value> Server::onCodeCompletion(json::Value value) {
    using CompletionParams = proto::TextDocumentPositionParams;
    auto params = json::deserialize<CompletionParams>(value);

    auto path = converter.convert(params.textDocument.uri);
    auto content = scheduler.getDocumentContent(path);
    auto offset = converter.convert(content, params.position);
    co_return co_await scheduler.completion(std::move(path), offset);
}

}  // namespace clice
