#include "Server/Config.h"
#include "Server/Scheduler.h"
#include "Server/LSPConverter.h"
#include "Support/Logger.h"
#include "Support/FileSystem.h"
#include "Compiler/Command.h"
#include "Compiler/Compilation.h"

namespace clice {

async::Task<> Scheduler::add_document(std::string path, std::string content) {
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
}

llvm::StringRef Scheduler::getDocumentContent(llvm::StringRef path) {
    return opening_files[path].content;
}

async::Task<json::Value> Scheduler::semantic_tokens(std::string path) {
    auto openFile = &opening_files[path];
    auto guard = co_await openFile->ast_built_lock.try_lock();

    openFile = &opening_files[path];
    auto content = openFile->content;
    auto ast = openFile->ast;
    if(!ast) {
        co_return json::Value(nullptr);
    }

    auto tokens = co_await async::submit([&] { return feature::semanticTokens(*ast); });

    co_return converter.convert(content, tokens);
}

async::Task<json::Value> Scheduler::completion(std::string path, std::uint32_t offset) {
    /// Wait for PCH building.
    auto openFile = &opening_files[path];
    if(!openFile->pch_build_task.empty()) {
        co_await openFile->pch_built_event;
    }

    openFile = &opening_files[path];
    auto& pch = openFile->pch;

    /// Set compilation params ... .
    CompilationParams params;
    params.arguments = database.get_command(path, true).arguments;
    params.add_remapped_file(path, openFile->content);
    params.pch = {pch->path, pch->preamble.size()};
    params.completion = {path, offset};

    auto result = co_await async::submit([&] { return feature::code_complete(params, {}); });

    openFile = &opening_files[path];
    co_return converter.convert(openFile->content, result);
}

async::Task<bool> Scheduler::isPCHOutdated(llvm::StringRef path, llvm::StringRef preamble) {
    auto openFile = &opening_files[path];

    /// If there is not PCH, directly build it.
    if(!openFile->pch) {
        co_return true;
    }

    /// Check command and preamble matchs.
    auto command = database.get_command(path, true).arguments;
    /// FIXME: check command. openFile->PCH->command != command
    if(openFile->pch->preamble != preamble) {
        co_return true;
    }

    /// TODO: Check mtime.

    co_return false;
}

async::Task<> Scheduler::build_pch(std::string path, std::string content) {
    auto bound = computePreambleBound(content);

    auto openFile = &opening_files[path];
    bool outdated = true;
    if(openFile->pch) {
        outdated = co_await isPCHOutdated(path, llvm::StringRef(content).substr(0, bound));
    }

    /// If not need update, return directly.
    if(!outdated) {
        co_return;
    }

    /// The actual PCH build task.
    constexpr static auto PCHBuildTask = [](Scheduler& scheduler,
                                            std::string path,
                                            std::uint32_t bound,
                                            std::string content) -> async::Task<> {
        CompilationParams params;
        params.arguments = scheduler.database.get_command(path, true).arguments;
        if(!fs::exists(config::cache.dir)) {
            auto error = fs::create_directories(config::cache.dir);
            if(error) {
                log::warn("Fail to create directory for PCH building: {}", config::cache.dir);
                co_return;
            }
        }

        params.outPath = path::join(config::cache.dir, path::filename(path) + ".pch");
        params.add_remapped_file(path, content, bound);

        PCHInfo info;

        /// PCH file is written until destructing, Add a single block
        /// for it.
        bool cond = co_await async::submit([&] {
            auto result = compile(params, info);
            if(!result) {
                /// log::warn("Building PCH fails for {}, Because: {}", path, result.error());
                return false;
            }

            /// TODO: index PCH.

            return true;
        });

        if(!cond) {
            co_return;
        }

        auto& openFile = scheduler.opening_files[path];
        /// Update the built PCH info.
        openFile.pch = std::move(info);
        /// Dispose the task so that it will destroyed when task complete.
        openFile.pch_build_task.dispose();
        /// Resume waiters on this event.
        openFile.pch_built_event.set();
        openFile.pch_built_event.clear();

        log::info("Building PCH successfully for {}", path);
    };

    openFile = &opening_files[path];

    /// If there is already an PCH build task, cancel it.
    auto& task = openFile->pch_build_task;
    if(!task.empty()) {
        task.cancel();
        task.dispose();
    }

    /// Schedule the new building task.
    task = PCHBuildTask(*this, std::move(path), bound, std::move(content));
    co_await task;
}

async::Task<> Scheduler::build_ast(std::string path, std::string content) {
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

    CompilationParams params;
    params.arguments = database.get_command(path, true).arguments;
    params.add_remapped_file(path, content);
    params.pch = {pch->path, pch->preamble.size()};

    /// Check result
    auto ast = co_await async::submit([&] { return compile(params); });
    if(!ast) {
        /// FIXME: Fails needs cancel waiting tasks.
        /// log::warn("Building AST fails for {}, Beacuse: {}", path, AST.error());
        co_return;
    }

    /// Index the source file.
    co_await indexer.index(*ast);

    file = &opening_files[path];
    /// Update built AST info.
    file->ast = std::make_shared<CompilationUnit>(std::move(*ast));
    /// Dispose the task so that it will destroyed when task complete.
    file->ast_build_task.dispose();

    log::info("Building AST successfully for {}", path);
}

}  // namespace clice
