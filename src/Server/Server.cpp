#include "Support/Logger.h"
#include "Server/Server.h"

namespace clice {

std::optional<OpenFile*> ActiveFileManager::get(llvm::StringRef path) {
    auto iter = index.find(path);
    if(iter == index.end()) {
        return std::nullopt;
    }

    /// If the file is in the chain, move it to the front.
    items.splice(items.begin(), items, iter->second);
    return std::addressof(iter->second->second);
}

OpenFile* ActiveFileManager::lru_put_impl(llvm::StringRef path, OpenFile file) {
    /// If the file is not in the chain, create a new OpenFile.
    if(items.size() >= max_size) {
        /// If the size exceeds the maximum size, remove the last element.
        index.erase(items.back().first);
        items.pop_back();
    }
    items.emplace_front(path, std::move(file));

    // fix the ownership of the StringRef of the path.
    auto [added, _] = index.insert({path, items.begin()});
    items.front().first = added->getKey();

    return std::addressof(items.front().second);
}

OpenFile* ActiveFileManager::get_or_create(llvm::StringRef path) {
    auto iter = index.find(path);
    if(iter == index.end()) {
        return lru_put_impl(path, OpenFile{});
    }

    // If the file is in the chain, move it to the front.
    items.splice(items.begin(), items, iter->second);
    return std::addressof(iter->second->second);
}

OpenFile* ActiveFileManager::put(llvm::StringRef path, OpenFile file) {
    auto iter = index.find(path);
    if(iter == index.end()) {
        return lru_put_impl(path, std::move(file));
    }
    iter->second->second = std::move(file);

    // If the file is in the chain, move it to the front.
    items.splice(items.begin(), items, iter->second);
    return std::addressof(iter->second->second);
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

Server::Server() {
    register_callback<&Server::on_initialize>("initialize");
    register_callback<&Server::on_initialized>("initialized");
    register_callback<&Server::on_shutdown>("shutdown");
    register_callback<&Server::on_exit>("exit");

    register_callback<&Server::on_did_open>("textDocument/didOpen");
    register_callback<&Server::on_did_change>("textDocument/didChange");
    register_callback<&Server::on_did_save>("textDocument/didSave");
    register_callback<&Server::on_did_close>("textDocument/didClose");

    register_callback<&Server::on_completion>("textDocument/completion");
    register_callback<&Server::on_hover>("textDocument/hover");
    register_callback<&Server::on_document_symbol>("textDocument/documentSymbol");
    register_callback<&Server::on_document_link>("textDocument/documentLink");
    register_callback<&Server::on_folding_range>("textDocument/foldingRange");
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
