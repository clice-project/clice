#include "Server/Scheduler.h"
#include "Support/FileSystem.h"
#include "Compiler/Compilation.h"

namespace clice {

void Scheduler::addDocument(std::string path, std::string content) {
    auto& task = openFiles[path].ASTBuild;

    /// If there is already an AST build task, cancel it.
    if(!task.empty()) {
        task.cancel();
        task.dispose();
    }

    /// Create and schedule a new task.
    task = buildAST(std::move(path), std::move(content));
    task.schedule();
}

async::Task<bool> Scheduler::isPCHOutdated(llvm::StringRef file, llvm::StringRef preamble) {
    co_return true;
}

async::Task<> Scheduler::buildPCH(std::string path, std::string content) {
    auto openFile = &openFiles[path];
    bool outdated = true;
    if(openFile->PCH) {
        outdated = co_await isPCHOutdated(path, content);
    }

    /// If not need update, return directly.
    if(!outdated) {
        co_return;
    }

    /// The actual PCH build task.
    constexpr static auto PCHBuildTask =
        [](Scheduler& scheduler, std::string path, std::string preamble) -> async::Task<> {
        auto command = scheduler.database.getCommand(path);

        CompilationParams params;
        params.command = command;
        params.srcPath = path;
        params.outPath = "...";
        params.content = preamble;

        PCHInfo info;
        auto result = co_await async::submit([&] { return compile(params, info); });

        auto& openFile = scheduler.openFiles[path];
        /// Update the built PCH info.
        openFile.PCH = std::move(info);
        /// Dispose the task so that it will destroyed when task complete.
        openFile.PCHBuild.dispose();
        /// Resume waiters on this event.
        openFile.PCHBuiltEvent.set();
    };

    openFile = &openFiles[path];

    /// If there is already an PCH build task, cancel it.
    auto& task = openFile->PCHBuild;
    if(!task.empty()) {
        task.cancel();
        task.dispose();
    }

    /// Schedule the new building task.
    task = PCHBuildTask(*this, std::move(path), std::move(content));
    task.schedule();

    /// Waiting for PCH building.
    co_await openFile->PCHBuiltEvent;
}

async::Task<> Scheduler::buildAST(std::string path, std::string content) {
    /// PCH is already updated.
    co_await buildPCH(path, content);

    auto command = database.getCommand(path);

    CompilationParams params;
    params.command = command;
    params.content = content;

    /// Check result
    auto info = co_await async::submit([&] { return compile(params); });
    if(!info) {}

    auto& file = openFiles[path];
    /// Update built AST info.
    file.AST = std::make_shared<ASTInfo>(std::move(*info));
    /// Dispose the task so that it will destroyed when task complete.
    file.ASTBuild.dispose();
    /// Resume waiters on this event.
    file.ASTBuiltEvent.set();
}

async::Task<feature::CodeCompletionResult> Scheduler::codeCompletion(llvm::StringRef path,
                                                                     llvm::StringRef content,
                                                                     std::uint32_t line,
                                                                     std::uint32_t column) {
    /// Wait for PCH building.
    auto& openFile = openFiles[path];
    if(!openFile.PCHBuild.empty()) {
        co_await openFile.PCHBuiltEvent;
    }

    /// Set compilation params ... .
    CompilationParams params;

    co_return co_await async::submit([&] { return feature::codeCompletion(params, {}); });
}

}  // namespace clice
