#pragma once

#include "Config.h"
#include "Indexer.h"
#include "Async/Async.h"
#include "Compiler/Command.h"
#include "Compiler/Preamble.h"
#include "Compiler/Diagnostic.h"
#include "Protocol/Protocol.h"

namespace clice {

struct OpenFile {
    /// The file version, every edition will increase it.
    std::uint32_t version = 0;

    /// The file content.
    std::string content;

    /// We build PCH for every opened file.
    std::optional<PCHInfo> pch;
    async::Task<> pch_build_task;
    async::Event pch_built_event;

    /// For each opened file, we would like to build an AST for it.
    std::shared_ptr<CompilationUnit> ast;
    async::Task<> ast_build_task;
    async::Lock ast_built_lock;

    /// Collect all diagnostics in the compilation.
    std::shared_ptr<std::vector<Diagnostic>> diagnostics =
        std::make_unique<std::vector<Diagnostic>>();

    /// For header with context, it may have multiple ASTs, use
    /// an chain to store them.
    std::unique_ptr<OpenFile> next;
};

struct PathMapping {
    std::string to_path(llvm::StringRef uri) {
        /// FIXME: Path mapping.
        return fs::toPath(uri);
    }

    std::string to_uri(llvm::StringRef path) {
        /// FIXME: Path mapping.
        return fs::toURI(path);
    }
};

class Server {
public:
    Server();

    using Self = Server;

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

    async::Task<> on_receive(json::Value value);

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
    async::Task<json::Value> on_initialize(proto::InitializeParams params);

private:
    async::Task<OpenFile*> add_document(std::string path, std::string content);

    async::Task<> build_pch(std::string file, std::string preamble);

    async::Task<> build_ast(std::string file, std::string content);

    async::Task<> on_did_open(proto::DidOpenTextDocumentParams params);

    async::Task<> on_did_change(proto::DidChangeTextDocumentParams params);

    async::Task<> on_did_save(proto::DidSaveTextDocumentParams params);

    async::Task<> on_did_close(proto::DidCloseTextDocumentParams params);

private:
    async::Task<json::Value> on_semantic_token(proto::SemanticTokensParams params);

    async::Task<json::Value> on_completion(proto::CompletionParams params);

private:
    /// The current request id.
    std::uint32_t id = 0;

    /// All registered LSP callbacks.
    llvm::StringMap<Callback> callbacks;

    proto::PositionEncodingKind kind;

    std::string workspace;

    /// The compilation database.
    CompilationDatabase database;

    /// All opening files, TODO: use a LRU cache.
    llvm::StringMap<OpenFile> opening_files;

    PathMapping mapping;
};

}  // namespace clice
