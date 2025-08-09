#include "Support/Logger.h"
#include "Server/Server.h"
#include "Compiler/Compilation.h"
#include "Feature/Diagnostic.h"

namespace clice {

async::Task<> Server::build_pch(std::string file, std::string content) {
    auto bound = compute_preamble_bound(content);

    auto open_file = &opening_files[file];
    auto info = database.get_command(file, true, true);

    auto check_pch_update = [&content, &bound, &info](PCHInfo& pch) {
        if(content.substr(0, bound) != pch.preamble) {
            return true;
        }

        if(info.arguments != pch.arguments) {
            return true;
        }

        /// Check deps.
        for(auto& dep: pch.deps) {
            fs::file_status status;
            auto error = fs::status(dep, status, true);
            if(error || std::chrono::duration_cast<std::chrono::milliseconds>(
                            status.getLastModificationTime().time_since_epoch())
                                .count() > pch.mtime) {
                return true;
            }
        }

        return false;
    };

    /// Check update ...
    if(open_file->pch && !check_pch_update(*open_file->pch)) {
        /// If not need update, return directly.
        log::info("PCH is already up-to-date for {}", file);
        co_return;
    }

    /// The actual PCH build task.
    constexpr static auto PCHBuildTask =
        [](CompilationDatabase::LookupInfo& info,
           OpenFile* open_file,
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
        params.output_file = path::join(config::cache.dir, path::filename(path) + ".pch");
        params.arguments = std::move(info.arguments);
        params.diagnostics = diagnostics;
        params.add_remapped_file(path, content, bound);

        PCHInfo pch;

        std::string command;
        for(auto argument: params.arguments) {
            command += " ";
            command += argument;
        }

        log::info("Start building PCH for {}, command: [{}]", path, command);

        std::string message;
        std::vector<feature::DocumentLink> links;

        bool success = co_await async::submit([&params, &pch, &message, &links] -> bool {
            /// PCH file is written until destructing, Add a single block
            /// for it.
            auto unit = compile(params, pch);
            if(!unit) {
                message = std::move(unit.error());
                return false;
            }

            links = feature::document_links(*unit);
            /// TODO: index PCH file, etc
            return true;
        });

        if(!success) {
            log::warn("Building PCH fails for {}, Because: {}", path, message);
            for(auto& diagnostic: *diagnostics) {
                log::warn("{}", diagnostic.message);
            }
            co_return false;
        }

        /// Update the built PCH info.
        open_file->pch = std::move(pch);
        open_file->pch_includes = std::move(links);

        /// Resume waiters on this event.
        open_file->pch_built_event.set();
        open_file->pch_built_event.clear();

        co_return true;
    };

    open_file = &opening_files[file];

    /// If there is already an PCH build task, cancel it.
    auto& task = open_file->pch_build_task;
    if(!task.empty()) {
        task.cancel();
        task.dispose();
    }

    /// Schedule the new building task.
    task = PCHBuildTask(info, open_file, file, bound, std::move(content), open_file->diagnostics);

    if(co_await task) {
        log::info("Building PCH successfully for {}", file);

        /// Dispose the task so that it will destroyed when task complete.
        task.dispose();
    }

    /// TODO: report diagnostics in the preamble.
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
    params.arguments = database.get_command(path, true, true).arguments;
    params.add_remapped_file(path, content);
    params.pch = {pch->path, pch->preamble.size()};
    file->diagnostics->clear();
    params.diagnostics = file->diagnostics;

    /// Check result
    auto ast = co_await async::submit([&] { return compile(params); });
    if(!ast) {
        /// FIXME: Fails needs cancel waiting tasks.
        log::warn("Building AST fails for {}, Beacuse: {}", path, ast.error());
        for(auto& diagnostic: *file->diagnostics) {
            log::warn("{}", diagnostic.message);
        }
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

async::Task<> Server::on_did_open(proto::DidOpenTextDocumentParams params) {
    auto path = mapping.to_path(params.textDocument.uri);
    auto file = co_await add_document(path, std::move(params.textDocument.text));
    if(file->diagnostics) {
        auto guard = co_await file->ast_built_lock.try_lock();
        file = &opening_files[path];
        auto diagnostics = feature::diagnostics(kind, mapping, *file->ast);
        co_await notify("textDocument/publishDiagnostics",
                        json::Object{
                            {"uri",         mapping.to_uri(path)  },
                            {"diagnostics", std::move(diagnostics)},
        });
    }
    co_return;
}

async::Task<> Server::on_did_change(proto::DidChangeTextDocumentParams params) {
    auto path = mapping.to_path(params.textDocument.uri);
    auto file = co_await add_document(path, std::move(params.contentChanges[0].text));
    if(file->diagnostics) {
        auto guard = co_await file->ast_built_lock.try_lock();
        file = &opening_files[path];
        auto diagnostics = feature::diagnostics(kind, mapping, *file->ast);
        co_await notify("textDocument/publishDiagnostics",
                        json::Object{
                            {"uri",         mapping.to_uri(path)  },
                            {"diagnostics", std::move(diagnostics)},
        });
    }
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
