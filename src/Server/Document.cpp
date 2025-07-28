#include "Support/Logger.h"
#include "Server/Server.h"
#include "Compiler/Compilation.h"

namespace clice {

async::Task<OpenFile*> Server::add_document(std::string path, std::string content) {
    auto& openFile = opening_files[path];
    openFile.content = content;

    auto& task = openFile.ast_build_task;

    /// If there is already an AST build task, cancel it.
    if(!task.empty()) {
        task.cancel();
        task.dispose();
    }

    /// Create and schedule a new task.
    task = build_ast(std::move(path), std::move(content));
    co_await task;

    co_return &opening_files[path];
}

async::Task<> Server::build_pch(std::string path, std::string content) {
    auto bound = computePreambleBound(content);

    auto openFile = &opening_files[path];
    bool outdated = true;
    if(openFile->pch) {
        /// FIXME:
        /// outdated = co_await isPCHOutdated(path, llvm::StringRef(content).substr(0, bound));
    }

    /// If not need update, return directly.
    if(!outdated) {
        co_return;
    }

    /// The actual PCH build task.
    constexpr static auto PCHBuildTask =
        [](Server& server,
           std::string path,
           std::uint32_t bound,
           std::string content,
           std::shared_ptr<std::vector<Diagnostic>> diagnostics) -> async::Task<bool> {
        if(!fs::exists(config::cache.dir)) {
            auto error = fs::create_directories(config::cache.dir);
            if(error) {
                log::warn("Fail to create directory for PCH building: {}", config::cache.dir);
                co_return false;
            }
        }

        /// Everytime we build a new pch, the old diagnostics should be discarded.
        diagnostics->clear();

        CompilationParams params;
        params.outPath = path::join(config::cache.dir, path::filename(path) + ".pch");
        params.arguments = server.database.get_command(path, true).arguments;
        params.diagnostics = diagnostics;
        params.add_remapped_file(path, content, bound);

        PCHInfo info;

        /// PCH file is written until destructing, Add a single block
        /// for it.
        bool cond = co_await async::submit([&] {
            auto result = compile(params, info);
            if(!result) {
                log::warn("Building PCH fails for {}, Because: {}", path, result.error());
                for(auto& diagnostic: *diagnostics) {
                    log::warn("{}", diagnostic.message);
                }
                return false;
            }

            /// TODO: index PCH.

            return true;
        });

        if(!cond) {
            co_return false;
        }

        auto& openFile = server.opening_files[path];
        /// Update the built PCH info.
        openFile.pch = std::move(info);
        /// Dispose the task so that it will destroyed when task complete.
        openFile.pch_build_task.dispose();
        /// Resume waiters on this event.
        openFile.pch_built_event.set();
        openFile.pch_built_event.clear();

        co_return true;
    };

    openFile = &opening_files[path];

    /// If there is already an PCH build task, cancel it.
    auto& task = openFile->pch_build_task;
    if(!task.empty()) {
        task.cancel();
        task.dispose();
    }

    /// Schedule the new building task.
    task = PCHBuildTask(*this, path, bound, std::move(content), openFile->diagnostics);

    log::info("Start building PCH for {}", path);

    if(co_await task) {
        log::info("Building PCH successfully for {}", path);
    }
}

async::Task<> Server::build_ast(std::string path, std::string content) {
    auto file = &opening_files[path];

    /// Try get the lock, the waiter on the lock will be resumed when
    /// guard is destroyed.
    auto guard = co_await file->ast_built_lock.try_lock();

    /// PCH is already updated.
    co_await build_pch(path, content);

    auto pch = opening_files[path].pch;
    if(!pch) {
        log::fatal("Expected PCH built at this point");
    }

    file = &opening_files[path];
    CompilationParams params;
    params.arguments = database.get_command(path, true).arguments;
    params.add_remapped_file(path, content);
    params.pch = {pch->path, pch->preamble.size()};
    params.diagnostics = file->diagnostics;

    /// Check result
    auto ast = co_await async::submit([&] { return compile(params); });
    if(!ast) {
        /// FIXME: Fails needs cancel waiting tasks.
        log::warn("Building AST fails for {}, Beacuse: {}", path, ast.error());
        co_return;
    }

    /// FIXME: Index the source file.
    /// co_await indexer.index(*ast);

    file = &opening_files[path];
    /// Update built AST info.
    file->ast = std::make_shared<CompilationUnit>(std::move(*ast));
    /// Dispose the task so that it will destroyed when task complete.
    file->ast_build_task.dispose();

    log::info("Building AST successfully for {}", path);
}

async::Task<> Server::on_did_open(proto::DidOpenTextDocumentParams params) {
    auto path = mapping.to_path(params.textDocument.uri);
    auto file = co_await add_document(std::move(path), std::move(params.textDocument.text));
    if(file->diagnostics) {
        /// Publish diagnostics here ...
    }
    co_return;
}

async::Task<> Server::on_did_change(proto::DidChangeTextDocumentParams params) {
    auto path = mapping.to_path(params.textDocument.uri);
    co_await add_document(std::move(path), std::move(params.contentChanges[0].text));
    co_return;
}

async::Task<> Server::on_did_save(proto::DidSaveTextDocumentParams params) {
    auto path = mapping.to_path(params.textDocument.uri);
    co_return;
}

async::Task<> Server::on_did_close(proto::DidCloseTextDocumentParams params) {
    auto path = mapping.to_path(params.textDocument.uri);
    co_return;
}

}  // namespace clice
