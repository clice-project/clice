#pragma once

#include "Config.h"
#include "Indexer.h"
#include "Scheduler.h"
#include "Async/Async.h"
#include "Compiler/Command.h"
#include "Protocol/Protocol.h"

namespace clice {

using async::Task;

class Server {
public:
    Server();

    Task<> onReceive(json::Value value);

private:
    /// Send a request to the client.
    Task<> request(llvm::StringRef method, json::Value params);

    /// Send a notification to the client.
    Task<> notify(llvm::StringRef method, json::Value params);

    /// Send a response to the client.
    Task<> response(json::Value id, json::Value result);

    Task<> response(json::Value id, proto::ErrorCodes code, llvm::StringRef message = "");

    /// Send an register capability to the client.
    Task<> registerCapacity(llvm::StringRef id,
                            llvm::StringRef method,
                            json::Value registerOptions);

private:
    Task<json::Value> onInitialize(json::Value value);

    Task<> onDidOpen(json::Value value);

    Task<> onDidChange(json::Value value);

    Task<> onDidSave(json::Value value);

    Task<> onDidClose(json::Value value);

    Task<json::Value> onSemanticToken(json::Value value);

    Task<json::Value> onCodeCompletion(json::Value value);

private:
    std::uint32_t id = 0;

    Indexer indexer;

    Scheduler scheduler;

    CompilationDatabase database;

    using OnRequest = async::Task<json::Value> (Server::*)(json::Value);
    using OnNotification = async::Task<> (Server::*)(json::Value);

    llvm::StringMap<OnRequest> onRequests;
    llvm::StringMap<OnNotification> onNotifications;
};

}  // namespace clice
