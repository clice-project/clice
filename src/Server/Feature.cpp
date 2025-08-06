#include "Feature/FoldingRange.h"
#include "Server/Server.h"
#include "Server/Convert.h"
#include "Compiler/Compilation.h"
#include "Feature/CodeCompletion.h"
#include "Feature/Hover.h"
#include "Feature/DocumentLink.h"
#include "Feature/SemanticToken.h"

namespace clice {

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

async::Task<json::Value> Server::on_hover(proto::HoverParams params) {
    auto path = mapping.to_path(params.textDocument.uri);

    auto opening_file = &opening_files[path];
    auto guard = co_await opening_file->ast_built_lock.try_lock();

    auto offset = to_offset(kind, opening_file->content, params.position);

    opening_file = &opening_files[path];
    auto content = opening_file->content;
    auto ast = opening_file->ast;
    if(!ast) {
        co_return json::Value(nullptr);
    }

    co_return co_await async::submit([kind = this->kind, offset, &ast] {
        auto hover = feature::hover(*ast, offset);

        proto::Hover result;
        result.contents.kind = "markdown";
        result.contents.value = std::format("{}: {}", hover.kind.name(), hover.name);

        return json::serialize(result);
    });
}

async::Task<json::Value> Server::on_document_link(proto::DocumentLinkParams params) {
    auto path = mapping.to_path(params.textDocument.uri);

    auto opening_file = &opening_files[path];
    auto guard = co_await opening_file->ast_built_lock.try_lock();

    opening_file = &opening_files[path];
    auto content = opening_file->content;
    auto ast = opening_file->ast;
    if(!ast) {
        co_return json::Value(nullptr);
    }

    auto pch_links = opening_file->pch_includes;
    auto mapping = this->mapping;

    co_return co_await async::submit([&, kind = this->kind] {
        auto links = feature::document_links(*ast);
        links.insert(links.begin(), pch_links.begin(), pch_links.end());

        PositionConverter converter(content, kind);
        converter.to_positions(links, [](feature::DocumentLink& link) { return link.range; });

        std::vector<proto::DocumentLink> result;
        for(auto& link: links) {
            result.emplace_back(converter.lookup(link.range), mapping.to_uri(link.file));
        }
        return json::serialize(result);
    });
}

async::Task<json::Value> Server::on_folding_range(proto::FoldingRangeParams params) {
    auto path = mapping.to_path(params.textDocument.uri);

    auto opening_file = &opening_files[path];
    auto guard = co_await opening_file->ast_built_lock.try_lock();

    opening_file = &opening_files[path];
    auto content = opening_file->content;
    auto ast = opening_file->ast;
    if(!ast) {
        co_return json::Value(nullptr);
    }

    co_return co_await async::submit([&, kind = this->kind] {
        auto foldings = feature::folding_ranges(*ast);
        PositionConverter converter(content, kind);
        converter.to_positions(foldings,
                               [](feature::FoldingRange& folding) { return folding.range; });

        std::vector<proto::FoldingRange> result;

        for(auto&& folding: foldings) {
            auto [begin_offset, end_offset] = folding.range;
            auto [begin_line, begin_char] = converter.lookup(begin_offset);
            auto [end_line, end_char] = converter.lookup(end_offset);

            proto::FoldingRange range;
            range.startLine = begin_line;
            range.startCharacter = begin_char;
            range.endLine = end_line;
            range.endCharacter = end_char;
            range.kind = "region";
            range.collapsedText = folding.text;
            result.emplace_back(std::move(range));
        }

        return json::serialize(result);
    });
}

async::Task<json::Value> Server::on_semantic_token(proto::SemanticTokensParams params) {
    auto path = mapping.to_path(params.textDocument.uri);

    auto opening_file = &opening_files[path];
    auto guard = co_await opening_file->ast_built_lock.try_lock();

    opening_file = &opening_files[path];
    auto content = opening_file->content;
    auto ast = opening_file->ast;
    if(!ast) {
        co_return json::Value(nullptr);
    }

    co_return co_await async::submit([kind = this->kind, &ast] {
        auto tokens = feature::semantic_tokens(*ast);
        return proto::to_json(kind, ast->interested_content(), tokens);
    });
}

}  // namespace clice
