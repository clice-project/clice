#pragma once

#include "Config.h"
#include "Indexer.h"
#include "Protocol.h"
#include "Scheduler.h"
#include "Async/Async.h"
#include "Compiler/Command.h"

namespace clice {

class Server {
public:
    Server();

    async::Task<> onReceive(json::Value value);

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
    async::Task<std::string> onInitialize(json::Value value);

    async::Task<> onDidOpen(json::Value value);

    async::Task<> onDidChange(json::Value value);

    async::Task<> onDidSave(json::Value value);

    async::Task<> onDidClose(json::Value value);

    async::Task<std::string> onSemanticToken(json::Value value);

    async::Task<std::string> onCodeCompletion(json::Value value);

private:
    std::uint32_t id = 0;
    Indexer indexer;
    Scheduler scheduler;
    CompilationDatabase database;

    using OnRequest = async::Task<std::string> (Server::*)(json::Value);
    using OnNotification = async::Task<> (Server::*)(json::Value);

    llvm::StringMap<OnRequest> onRequests;
    llvm::StringMap<OnNotification> onNotifications;
};

}  // namespace clice
