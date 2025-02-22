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

private:
    /// Send a request to the client.
    async::Task<> request(llvm::StringRef method, json::Value params);

    /// Send a notification to the client.
    async::Task<> notify(llvm::StringRef method, json::Value params);

    /// Send a response to the client.
    async::Task<> response(json::Value id, json::Value result);

    /// Send an register capability to the client.
    async::Task<> registerCapacity(llvm::StringRef id,
                                   llvm::StringRef method,
                                   json::Value registerOptions);

    async::Task<> initialize(const proto::InitializedParams& params);

public:
    std::uint32_t id = 0;
    // Indexer indexer;
    // LSPConverter& converter;

    /// SourceConverter converter;
    /// CompilationDatabase database;
    /// Indexer indexer;
    /// Scheduler scheduler;
};

}  // namespace clice
