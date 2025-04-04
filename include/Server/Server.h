#pragma once

#include "Config.h"
#include "Indexer.h"
#include "Protocol.h"
#include "LSPConverter.h"

#include "Async/Async.h"
#include "Compiler/Command.h"

namespace clice {

class Server {
public:
    Server();

    async::Task<> onReceive(json::Value value);

    /// Handle requests, a request must have a response.
    async::Task<json::Value> onRequest(llvm::StringRef method, json::Value value);

    /// Handle requests started with `textDocument/`.
    async::Task<json::Value> onTextDocument(llvm::StringRef method, json::Value value);

    /// Handle requests started with `context/`.
    async::Task<json::Value> onContext(llvm::StringRef method, json::Value value);

    /// Handle requests started with `index/`.
    async::Task<json::Value> onIndex(llvm::StringRef method, json::Value value);

    /// Handle notifications, a notification doesn't require response.
    async::Task<> onNotification(llvm::StringRef method, json::Value value);

    /// Handle notifications `context/
    async::Task<> onFileOperation(llvm::StringRef method, json::Value value);

private:
    /// Send a request to the client.
    async::Task<> request(llvm::StringRef method, json::Value params);

    /// Send a notification to the client.
    async::Task<> notify(llvm::StringRef method, json::Value params);

    /// Send a response to the client.
    async::Task<> response(json::Value id, json::Value result);

    async::Task<> response(json::Value id, proto::ErrorCodes code, llvm::StringRef message = "");

    /// Send an register capability to the client.
    async::Task<> registerCapacity(llvm::StringRef id,
                                   llvm::StringRef method,
                                   json::Value registerOptions);

private:

public:
    std::uint32_t id = 0;
    Indexer indexer;
    LSPConverter converter;
    CompilationDatabase database;
};

}  // namespace clice
