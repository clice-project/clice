#pragma once

#include "Config.h"
#include "Indexer.h"
#include "Protocol.h"
#include "LSPConverter.h"

#include "Async/Async.h"
#include "Compiler/Command.h"

namespace clice {

namespace proto {

enum class ErrorCodes {
    // Defined by JSON-RPC
    ParseError = -32700,
    InvalidRequest = -32600,
    MethodNotFound = -32601,
    InvalidParams = -32602,
    InternalError = -32603,

    /**
     * Error code indicating that a server received a notification or
     * request before the server has received the `initialize` request.
     */
    ServerNotInitialized = -32002,
    UnknownErrorCode = -32001,

    /**
     * A request failed but it was syntactically correct, e.g the
     * method name was known and the parameters were valid. The error
     * message should contain human readable information about why
     * the request failed.
     *
     * @since 3.17.0
     */
    RequestFailed = -32803,

    /**
     * The server cancelled the request. This error code should
     * only be used for requests that explicitly support being
     * server cancellable.
     *
     * @since 3.17.0
     */
    ServerCancelled = -32802,

    /**
     * The server detected that the content of a document got
     * modified outside normal conditions. A server should
     * NOT send this error code if it detects a content change
     * in it unprocessed messages. The result even computed
     * on an older state might still be useful for the client.
     *
     * If a client decides that a result is not of any use anymore
     * the client should cancel the request.
     */
    ContentModified = -32801,

    /**
     * The client has canceled a request and a server has detected
     * the cancel.
     */
    RequestCancelled = -32800,
};

}

class Server {
public:
    Server();

    async::Task<> onReceive(json::Value value);

    async::Task<json::Value> handleRequest(llvm::StringRef name, json::Value params);

    async::Task<> handleNotification(llvm::StringRef name, json::Value value);

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
    async::Task<> initialize(json::Value value);

public:
    std::uint32_t id = 0;
    // Indexer indexer;
    // LSPConverter& converter;

    /// SourceConverter converter;
    /// CompilationDatabase database;
    /// Indexer indexer;
    /// Scheduler scheduler;

    LSPConverter converter;
};

}  // namespace clice
