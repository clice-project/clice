#include "Server/Server.h"

namespace clice {

async::Task<json::Value> Server::on_semantic_token(proto::SemanticTokensParams params) {
    /// auto path = converter.convert(params.textDocument.uri);
    // co_return co_await scheduler.semantic_tokens(std::move(path));
    co_return json::Value(nullptr);
}

async::Task<json::Value> Server::on_completion(proto::CompletionParams params) {
    // auto path = converter.convert(params.textDocument.uri);
    // auto content = scheduler.getDocumentContent(path);
    // auto offset = converter.convert(content, params.position);
    // co_return co_await scheduler.completion(std::move(path), offset);
    co_return json::Value(nullptr);
}

}  // namespace clice
