#pragma once

#include <Server/Scheduler.h>
#include <Protocol/Protocol.h>
#include <llvm/ADT/FunctionExtras.h>

namespace clice {

struct Server {
    using Handler = llvm::unique_function<void(json::Value, json::Value)>;
    Scheduler scheduler;
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
    auto initialize(proto::InitializeParams params) -> proto::InitializeResult;

    void initialized(proto::InitializedParams params);
};

}  // namespace clice
