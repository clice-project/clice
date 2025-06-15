#include "Server/Config.h"
#include "Server/Scheduler.h"
#include "Server/LSPConverter.h"
#include "Support/Logger.h"
#include "Support/FileSystem.h"
#include "Compiler/Command.h"
#include "Compiler/Compilation.h"

namespace clice {

void Scheduler::addDocument(std::string path, std::string content) {
    auto& openFile = openFiles[path];
    openFile.content = content;

    auto& task = openFile.ASTBuild;

    /// If there is already an AST build task, cancel it.
    if(!task.empty()) {
        task.cancel();
        task.dispose();
    }

    /// Create and schedule a new task.
    task = buildAST(std::move(path), std::move(content));
    task.schedule();
}

llvm::StringRef Scheduler::getDocumentContent(llvm::StringRef path) {
    return openFiles[path].content;
}

async::Task<json::Value> Scheduler::semanticToken(std::string path) {
    auto openFile = &openFiles[path];
    auto guard = co_await openFile->ASTBuiltLock.try_lock();

    openFile = &openFiles[path];
    auto content = openFile->content;
    auto AST = openFile->AST;

    auto tokens = co_await async::submit([&] { return feature::semanticTokens(*AST); });

    co_return converter.convert(content, tokens);
}

async::Task<json::Value> Scheduler::completion(std::string path, std::uint32_t offset) {
    /// Wait for PCH building.
    auto openFile = &openFiles[path];
    if(!openFile->PCHBuild.empty()) {
        co_await openFile->PCHBuiltEvent;
    }

    openFile = &openFiles[path];
    auto& PCH = openFile->PCH;

    /// Set compilation params ... .
    CompilationParams params;
    params.command = database.getCommand(path);
    params.srcPath = path;
    params.content = openFile->content;
    params.pch = {PCH->path, PCH->preamble.size()};
    params.completion = {path, offset};

    auto result = co_await async::submit([&] { return feature::codeCompletion(params, {}); });

    openFile = &openFiles[path];
    co_return converter.convert(openFile->content, result);
}

async::Task<bool> Scheduler::isPCHOutdated(llvm::StringRef path, llvm::StringRef preamble) {
    auto openFile = &openFiles[path];

    /// If there is not PCH, directly build it.
    if(!openFile->PCH) {
        co_return true;
    }

    /// Check command and preamble matchs.
    auto command = database.getCommand(path);
    if(openFile->PCH->command != command || openFile->PCH->preamble != preamble) {
        co_return true;
    }

    /// TODO: Check mtime.

    co_return false;
}

async::Task<> Scheduler::buildPCH(std::string path, std::string content) {
    auto bound = computePreambleBound(content);

    auto openFile = &openFiles[path];
    bool outdated = true;
    if(openFile->PCH) {
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
        params.srcPath = path;
        params.command = scheduler.database.getCommand(path);
        params.content = content;
        params.bound = bound;
        params.outPath = path::join(config::index.dir, path::filename(path) + ".pch");

        PCHInfo info;
        auto result = co_await async::submit([&] { return compile(params, info); });
        if(!result) {
            /// FIXME: Fails needs cancel waiting tasks.
            log::warn("Building PCH fails for {}", path);
            co_return;
        }

        auto& openFile = scheduler.openFiles[path];
        /// Update the built PCH info.
        openFile.PCH = std::move(info);
        /// Dispose the task so that it will destroyed when task complete.
        openFile.PCHBuild.dispose();
        /// Resume waiters on this event.
        openFile.PCHBuiltEvent.set();
        openFile.PCHBuiltEvent.clear();

        log::info("Building PCH successfully for {}", path);
    };

    openFile = &openFiles[path];

    /// If there is already an PCH build task, cancel it.
    auto& task = openFile->PCHBuild;
    if(!task.empty()) {
        task.cancel();
        task.dispose();
    }

    /// Schedule the new building task.
    task = PCHBuildTask(*this, std::move(path), bound, std::move(content));
    task.schedule();

    /// Waiting for PCH building.
    co_await openFile->PCHBuiltEvent;
}

async::Task<> Scheduler::buildAST(std::string path, std::string content) {
    auto file = &openFiles[path];

    /// Try get the lock, the waiter on the lock will be resumed when
    /// guard is destroyed.
    auto guard = co_await file->ASTBuiltLock.try_lock();

    /// PCH is already updated.
    co_await buildPCH(path, content);

    auto PCH = openFiles[path].PCH;
    if(!PCH) {
        log::fatal("Expected PCH built at this point");
    }

    CompilationParams params;
    params.srcPath = path;
    params.command = database.getCommand(path);
    params.content = content;
    params.pch = {PCH->path, PCH->preamble.size()};

    /// Check result
    auto info = co_await async::submit([&] { return compile(params); });
    if(!info) {
        /// FIXME: Fails needs cancel waiting tasks.
        log::warn("Building AST fails for {}", path);
        co_return;
    }

    /// Index the source file.
    co_await indexer.index(*info);

    file = &openFiles[path];
    /// Update built AST info.
    file->AST = std::make_shared<CompilationUnit>(std::move(*info));
    /// Dispose the task so that it will destroyed when task complete.
    file->ASTBuild.dispose();

    log::info("Building AST successfully for {}", path);
}

}  // namespace clice
