#pragma once

#include <optional>
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
    Task<json::Value> on_initialize(proto::InitializeParams params);

    Task<> on_did_open(proto::DidOpenTextDocumentParams params);

    Task<> on_did_change(proto::DidChangeTextDocumentParams params);

    Task<> on_did_save(proto::DidSaveTextDocumentParams params);

    Task<> on_did_close(proto::DidCloseTextDocumentParams params);

    Task<json::Value> on_semantic_token(proto::SemanticTokensParams params);

    Task<json::Value> on_completion(proto::CompletionParams params);

private:
    using Callback = async::Task<json::Value> (*)(Server&, json::Value);

    template <auto method>
    void register_callback(llvm::StringRef name) {
        using MF = decltype(method);
        static_assert(std::is_member_function_pointer_v<MF>, "");
        using F = member_type_t<MF>;
        using Ret = function_return_t<F>;
        using Params = std::tuple_element_t<0, function_args_t<F>>;

        Callback callback = [](Server& server, json::Value value) -> async::Task<json::Value> {
            if constexpr(std::is_same_v<Ret, async::Task<>>) {
                co_await (server.*method)(json::deserialize<Params>(value));
                co_return json::Value(nullptr);
            } else {
                co_return co_await (server.*method)(json::deserialize<Params>(value));
            }
        };

        callbacks.try_emplace(name, callback);
    }

    std::uint32_t id = 0;

    Indexer indexer;

    Scheduler scheduler;

    CompilationDatabase database;

    llvm::StringMap<Callback> callbacks;
};

}  // namespace clice
