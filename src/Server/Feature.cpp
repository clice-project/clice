#include "Server/Server.h"
#include "Server/Convert.h"
#include "Compiler/Compilation.h"

namespace clice {

async::Task<json::Value> Server::on_semantic_token(proto::SemanticTokensParams params) {
    /// auto path = converter.convert(params.textDocument.uri);
    // co_return co_await scheduler.semantic_tokens(std::move(path));
    std::string path;  /// FIXME:

    auto openFile = &opening_files[path];
    auto guard = co_await openFile->ast_built_lock.try_lock();

    openFile = &opening_files[path];
    auto content = openFile->content;
    auto ast = openFile->ast;
    if(!ast) {
        co_return "";
    }

    co_return co_await async::submit([kind = this->kind, &ast] {
        auto tokens = feature::semanticTokens(*ast);
        return proto::to_json(kind, ast->interested_content(), tokens);
    });
    co_return json::Value(nullptr);
}

async::Task<json::Value> Server::on_completion(proto::CompletionParams params) {
    // auto path = converter.convert(params.textDocument.uri);
    // auto content = scheduler.getDocumentContent(path);
    // auto offset = converter.convert(content, params.position);
    // co_return co_await scheduler.completion(std::move(path), offset);
    std::string path;  /// FIXME:
    std::uint32_t offset = 0;

    auto openFile = &opening_files[path];
    if(!openFile->pch_build_task.empty()) {
        co_await openFile->pch_built_event;
    }

    openFile = &opening_files[path];
    auto& pch = openFile->pch;

    {
        /// Set compilation params ... .
        CompilationParams params;
        params.arguments = database.get_command(path, true).arguments;
        params.add_remapped_file(path, openFile->content);
        params.pch = {pch->path, pch->preamble.size()};
        params.completion = {path, offset};

        co_return co_await async::submit([kind = this->kind, content = openFile->content, &params] {
            auto items = feature::code_complete(params, {});
            return proto::to_json(kind, content, items);
        });
    }

    co_return json::Value(nullptr);
}

}  // namespace clice
