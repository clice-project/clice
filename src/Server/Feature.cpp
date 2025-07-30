#include "Server/Server.h"
#include "Server/Convert.h"
#include "Compiler/Compilation.h"

namespace clice {

async::Task<json::Value> Server::on_semantic_token(proto::SemanticTokensParams params) {
    auto path = mapping.to_path(params.textDocument.uri);

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
}

async::Task<json::Value> Server::on_completion(proto::CompletionParams params) {
    auto path = mapping.to_path(params.textDocument.uri);
    auto opening_file = &opening_files[path];
    auto offset = to_offset(kind, opening_file->content, params.position);

    if(!opening_file->pch_build_task.empty()) {
        co_await opening_file->pch_built_event;
    }

    opening_file = &opening_files[path];
    auto& pch = opening_file->pch;

    {
        /// Set compilation params ... .
        CompilationParams params;
        params.arguments = database.get_command(path, true).arguments;
        params.add_remapped_file(path, opening_file->content);
        params.pch = {pch->path, pch->preamble.size()};
        params.completion = {path, offset};

        co_return co_await async::submit(
            [kind = this->kind, content = opening_file->content, &params] {
                auto items = feature::code_complete(params, {});
                return proto::to_json(kind, content, items);
            });
    }
}

}  // namespace clice
