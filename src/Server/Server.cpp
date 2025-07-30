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

Server::Server() {
    register_callback<&Server::on_initialize>("initialize");

    register_callback<&Server::on_did_open>("textDocument/didOpen");
    register_callback<&Server::on_did_change>("textDocument/didChange");
    register_callback<&Server::on_did_save>("textDocument/didSave");
    register_callback<&Server::on_did_close>("textDocument/didClose");

    register_callback<&Server::on_completion>("textDocument/completion");
    register_callback<&Server::on_semantic_token>("textDocument/semanticTokens/full");
}

async::Task<> Server::on_receive(json::Value value) {
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
    auto it = callbacks.find(method);
    if(it == callbacks.end()) {
        log::info("Ignore unhandled method: {}", method);
        co_return;
    }

    if(id) {
        log::info("Handling request: {}", method);
        auto result = co_await it->second(*this, std::move(params));
        co_await response(std::move(*id), std::move(result));
        log::info("Handled request: {}", method);
    } else {
        log::info("Handling notification: {}", method);
        auto result = co_await it->second(*this, std::move(params));
        log::info("Handled notification: {}", method);
    }

    co_return;
}

}  // namespace clice
