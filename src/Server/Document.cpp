#include "Support/Logger.h"
#include "Server/Server.h"
#include "Compiler/Compilation.h"
#include "Feature/Diagnostic.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/FileOutputBuffer.h"
#include "llvm/ADT/ScopeExit.h"

namespace clice {

void Server::load_cache_info() {
    auto path = path::join(config::cache.dir, "cache.json");
    auto file = llvm::MemoryBuffer::getFile(path);
    if(!file) {
        log::warn("Fail to load cache info, because: {}", file.getError());
        return;
    }

    llvm::StringRef content = file.get()->getBuffer();
    auto json = json::parse(content);
    if(!json) {
        log::warn("Fail to load cache info, invalid json: {}", json.takeError());
        return;
    }

    auto object = json->getAsObject();
    if(!object) {
        return;
    }

    auto version = object->getString("version");
    if(!version) {
        log::info("Fail to load cache info, the cache info is outdated");
        return;
    }

    if(auto array = object->getArray("pchs")) {
        for(auto& pch: *array) {
            auto object = pch.getAsObject();
            if(!object) {
                continue;
            }

            auto file = object->getString("file");
            auto path = object->getString("path");
            auto preamble = object->getString("preamble");
            auto mtime = object->getNumber("mtime");
            auto deps = object->getArray("deps");
            auto arguments = object->getArray("arguments");
            auto includes = object->get("includes");

            if(!file || !path || !preamble || !mtime || !deps || !arguments || !includes) {
                continue;
            }

            PCHInfo info;
            info.path = *path;
            info.preamble = *preamble;
            info.mtime = *mtime;

            for(auto& dep: *deps) {
                info.deps.push_back(dep.getAsString()->str());
            }

            for(auto& argument: *arguments) {
                auto carg = database.save_string(*argument.getAsString());
                info.arguments.emplace_back(carg.data());
            }

            /// Update the PCH info.
            auto opening_file = opening_files.get_or_add(*file);
            opening_file->pch = std::move(info);
            opening_file->pch_includes =
                json::deserialize<decltype(opening_file->pch_includes)>(*includes);
        }
    }

    log::info("Load cache info successfully");
}

void Server::save_cache_info() {
    json::Object json;
    json["version"] = "0.0.1";
    json["pchs"] = json::Array();

    for(auto& [file, open_file]: opening_files) {
        if(!open_file->pch) {
            continue;
        }

        auto& pch = *open_file->pch;
        json::Object object;
        object["file"] = file;
        object["path"] = pch.path;
        object["preamble"] = pch.preamble;
        object["mtime"] = pch.mtime;
        object["deps"] = json::serialize(pch.deps);
        object["arguments"] = json::serialize(pch.arguments);
        object["includes"] = json::serialize(open_file->pch_includes);

        json["pchs"].getAsArray()->emplace_back(std::move(object));
    }

    auto final_path = path::join(config::cache.dir, "cache.json");

    llvm::SmallString<128> temp_path;
    if(auto error = llvm::sys::fs::createTemporaryFile("cache", "json", temp_path)) {
        log::warn("Fail to create temporary file for cache info: {}", error.message());
        return;
    }

    auto clean_up = llvm::make_scope_exit([&temp_path]() {
        if(auto errc = llvm::sys::fs::remove(temp_path)) {
            log::warn("Fail to remove temporary file: {}", errc.message());
        }
    });

    std::error_code EC;
    llvm::raw_fd_ostream os(temp_path, EC, llvm::sys::fs::OF_None);
    if(EC) {
        log::warn("Fail to open temporary file for writing: {}", EC.message());
        return;
    }

    os << json::Value(std::move(json));
    os.flush();
    os.close();

    if(os.has_error()) {
        log::warn("Fail to write cache info to temporary file");
        return;
    }

    if(auto error = llvm::sys::fs::rename(temp_path, final_path)) {
        log::warn("Fail to rename temporary file to final cache file: {}", error.message());
        return;
    }

    clean_up.release();

    log::info("Save cache info successfully");
}

namespace {

bool check_pch_update(llvm::StringRef content,
                      std::uint32_t bound,
                      CompilationDatabase::LookupInfo& info,
                      PCHInfo& pch) {
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
}

/// The actual PCH build task.
async::Task<bool> build_pch_task(CompilationDatabase::LookupInfo& info,
                                 std::shared_ptr<OpenFile> open_file,
                                 std::string path,
                                 std::uint32_t bound,
                                 std::string content,
                                 std::shared_ptr<std::vector<Diagnostic>> diagnostics) {
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
    params.kind = CompilationUnit::Preamble;
    params.output_file = path::join(config::cache.dir, path::filename(path) + ".pch");
    params.arguments = std::move(info.arguments);
    params.diagnostics = diagnostics;
    params.add_remapped_file(path, content, bound);

    std::string command;
    for(auto argument: params.arguments) {
        command += " ";
        command += argument;
    }

    log::info("Start building PCH for {}, command: [{}]", path, command);
    command.clear();

    PCHInfo pch;
    std::string message = std::move(command);  // reuse buffer
    std::vector<feature::DocumentLink> links;

    bool success = co_await async::submit([&params, &pch, &message, &links] -> bool {
        /// PCH file is written until destructing, Add a single block for it.
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

    log::info("Building PCH successfully for {}", path);

    /// Update the built PCH info.
    open_file->pch = std::move(pch);
    open_file->pch_includes = std::move(links);

    /// Resume waiters on this event.
    open_file->pch_built_event.set();
    open_file->pch_built_event.clear();

    co_return true;
};

}  // namespace

async::Task<bool> Server::build_pch(std::string file, std::string content) {
    CommandOptions options;
    options.resource_dir = true;
    options.query_driver = true;
    auto info = database.get_command(file, options);

    auto bound = compute_preamble_bound(content);
    auto& open_file = opening_files.get_or_add(file);

    /// Check update ...
    if(open_file->pch && !check_pch_update(content, bound, info, *open_file->pch)) {
        /// If not need update, return directly.
        log::info("PCH is already up-to-date for {}", file);
        co_return true;
    }

    /// If there is already an PCH build task, cancel it.
    auto& task = open_file->pch_build_task;
    if(!task.empty()) {
        if(task.finished()) {
            task.release().destroy();
            log::info("Release old pch task!");
        } else {
            task.cancel();
            task.dispose();
        }
        log::info("Cancel old PCH building task!");
    }

    /// Schedule the new building task.
    task = build_pch_task(info, open_file, file, bound, std::move(content), open_file->diagnostics);
    if(co_await task) {
        /// FIXME: At this point, task has already been finished, destroy it directly.
        task.release().destroy();
        co_return true;
    }

    /// FIXME: report diagnostics in the preamble.
    co_return false;
}

async::Task<> Server::build_ast(std::string path, std::string content) {
    auto file = opening_files.get_or_add(path);

    /// Try get the lock, the waiter on the lock will be resumed when
    /// guard is destroyed.
    auto guard = co_await file->ast_built_lock.try_lock();

    /// PCH is already updated.
    bool success = co_await build_pch(path, content);
    if(!success) {
        co_return;
    }

    auto pch = file->pch;
    if(!pch) {
        log::fatal("Expected PCH built at this point");
    }

    CommandOptions options;
    options.resource_dir = true;
    options.query_driver = true;

    CompilationParams params;
    params.kind = CompilationUnit::Content;
    params.arguments = database.get_command(path, options).arguments;
    params.add_remapped_file(path, content);
    params.pch = {pch->path, pch->preamble.size()};
    file->diagnostics->clear();
    params.diagnostics = file->diagnostics;
    params.clang_tidy = config::server.clang_tidy;

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

    /// Send diagnostics
    auto diagnostics = co_await async::submit(
        [&, kind = this->kind] { return feature::diagnostics(kind, mapping, *ast); });
    co_await notify("textDocument/publishDiagnostics",
                    json::Object{
                        {"uri",         mapping.to_uri(path)  },
                        {"diagnostics", std::move(diagnostics)},
    });

    /// FIXME: Index the source file.
    /// co_await indexer.index(*ast);

    /// Update built AST info.
    file->ast = std::make_shared<CompilationUnit>(std::move(*ast));

    /// Dispose the task so that it will destroyed when task complete.
    file->ast_build_task.dispose();

    log::info("Building AST successfully for {}", path);
}

async::Task<std::shared_ptr<OpenFile>> Server::add_document(std::string path, std::string content) {
    auto& openFile = opening_files.get_or_add(path);
    openFile->version += 1;
    openFile->content = content;

    auto& task = openFile->ast_build_task;

    /// If there is already an AST build task, cancel it.
    if(!task.empty()) {
        if(task.finished()) {
            task.release().destroy();
            log::info("Release old AST building Task!");
        } else {
            task.cancel();
            task.dispose();
        }
        log::info("Cancel old AST building Task!");
    }

    /// Create and schedule a new task.
    task = build_ast(std::move(path), std::move(content));
    task.schedule();

    co_return openFile;
}

async::Task<> Server::on_did_open(proto::DidOpenTextDocumentParams params) {
    auto path = mapping.to_path(params.textDocument.uri);
    auto file = co_await add_document(path, std::move(params.textDocument.text));
    co_return;
}

async::Task<> Server::on_did_change(proto::DidChangeTextDocumentParams params) {
    auto path = mapping.to_path(params.textDocument.uri);
    auto file = co_await add_document(path, std::move(params.contentChanges[0].text));
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
