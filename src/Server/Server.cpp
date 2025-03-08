#include "Support/Logger.h"
#include "Server/Server.h"

namespace clice {

Server::Server() : indexer(database, config::index) {}

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
        auto result = co_await onRequest(method, std::move(params));
        co_await response(std::move(*id), std::move(result));
        log::info("Handled request: {}", method);
    } else {
        log::info("Handling notification: {}", method);
        co_await onNotification(method, std::move(params));
        log::info("Handled notification: {}", method);
    }

    co_return;
}

async::Task<json::Value> Server::onRequest(llvm::StringRef method, json::Value value) {
    if(method == "initialize") {
        auto result = converter.initialize(std::move(value));
        config::init(converter.workspace());

        /// FIXME: Use a better way to handle compile commands.
        for(auto&& dir: config::server.compile_commands_dirs) {
            database.updateCommands(dir + "/compile_commands.json");
        }

        indexer.load();

        co_return json::serialize(result);
    } else if(method.starts_with("exit")) {
        indexer.save();
    } else if(method.consume_front("textDocument/")) {
        co_return co_await onTextDocument(method, std::move(value));
    } else if(method.consume_front("context/")) {
        co_return co_await onIndex(method, std::move(value));
    } else if(method.consume_front("index/")) {
        co_return co_await onContext(method, std::move(value));
    }

    co_return json::Value(nullptr);
}

async::Task<json::Value> Server::onTextDocument(llvm::StringRef method, json::Value value) {
    if(method == "semanticTokens/full") {
        auto params2 = json::deserialize<proto::SemanticTokensParams>(value);
        auto path = SourceConverter::toPath(params2.textDocument.uri);
        auto tokens = co_await indexer.semanticTokens(path);
        co_return co_await converter.convert(path, tokens);
    } else if(method == "foldingRange") {
        auto params2 = json::deserialize<proto::FoldingRangeParams>(value);
        auto path = SourceConverter::toPath(params2.textDocument.uri);
        auto foldings = co_await indexer.foldingRanges(path);
        co_return co_await converter.convert(path, foldings);
    }

    co_return json::Value(nullptr);
}

async::Task<json::Value> Server::onContext(llvm::StringRef method, json::Value value) {
    if(method == "current") {
        auto param2 = json::deserialize<proto::TextDocumentParams>(value);
        auto path = SourceConverter::toURI(param2.textDocument.uri);
        // auto result = indexer.currentContext(path);
        // co_return result.valid() ? json::serialize(result) : json::Value(nullptr);
    } else if(method == "switch") {
    } else if(method == "all") {
        auto param2 = json::deserialize<proto::TextDocumentParams>(value);
        auto path = SourceConverter::toURI(param2.textDocument.uri);
        /// auto result = indexer.contextAll(path);
    } else if(method == "resolve") {
        /// indexer.contextResolve()
    }

    co_return json::Value(nullptr);
}

async::Task<json::Value> Server::onIndex(llvm::StringRef method, json::Value value) {
    co_return json::Value(nullptr);
}

async::Task<> Server::onNotification(llvm::StringRef method, json::Value value) {
    if(method.consume_front("index/")) {
        if(method == "all") {
            indexer.indexAll();
        }
    }

    co_return;
}

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

}  // namespace clice
