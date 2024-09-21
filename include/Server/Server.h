#pragma once

#include <Server/Command.h>
#include <Server/Scheduler.h>
#include <Protocol/Protocol.h>

namespace clice {

struct Server {
    using Handler = llvm::unique_function<void(json::Value, json::Value)>;
    Scheduler scheduler;
    CompilationDatabase CDB;
    llvm::StringMap<Handler> handlers;

    static Server instance;

    Server();

    int run(int argc, const char** argv);

    void handleMessage(std::string_view message);

    void notify();

    void response(json::Value id, json::Value result);

public:
    // LSP methods, if the return type is void, the method is used for notification.
    // otherwise, the method is used for request.
    auto initialize(protocol::InitializeParams params) -> protocol::InitializeResult;

    void initialized(protocol::InitializedParams params);
};

}  // namespace clice
