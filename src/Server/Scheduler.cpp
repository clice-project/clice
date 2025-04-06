#include "Server/Scheduler.h"
#include "Support/FileSystem.h"
#include "Compiler/Compilation.h"

namespace clice {

void Scheduler::addDocument(std::string file, std::string content) {
    auto task = buildAST(file, content);
    task.schedule();
    task.dispose();
}

async::Task<bool> Scheduler::checkPCHUpdate(llvm::StringRef file, llvm::StringRef preamble) {
    co_return true;
}

async::Task<> Scheduler::buildPCH(std::string path, std::string preamble) {
    auto file = &files[path];
    bool needRebuildPCH = true;
    if(file->PCH) {
        needRebuildPCH = co_await checkPCHUpdate(path, preamble);
    }

    constexpr static auto PCHBuildTask =
        [](Scheduler& self, std::string file, std::string preamble) -> async::Task<> {
        auto command = self.database.getCommand(file);
        CompilationParams params;
        params.command = command;
        params.srcPath = file;
        params.outPath = "...";
        params.content = preamble;

        auto result = co_await async::submit([&] {
            PCHInfo info;
            return compile(params, info);
        });
    };

    if(needRebuildPCH) {
        file = &files[path];

        /// If we need to build an new PCH, cancel older one.
        if(!file->PCHBuild.empty()) {
            file->PCHBuild.cancel();
            file->PCHBuild.dispose();
        }

        /// Schedule the new building task.
        file->PCHBuild = PCHBuildTask(*this, std::move(path), std::move(preamble));
        file->PCHBuild.schedule();

        /// Waiting for PCH building.
        co_await file->PCHBuiltEvent;
    }
}

async::Task<> Scheduler::buildAST(std::string path, std::string content) {
    /// PCH is already updated.
    co_await buildPCH(path, content);

    auto command = database.getCommand(path);

    CompilationParams params;
    params.command = command.str();
    params.content = content;

    /// Check result
    auto result = co_await async::submit([&] { return compile(params); });

    auto& file = files[path];
    file.AST = std::make_shared<ASTInfo>(std::move(*result));

    file.ASTBuild.dispose();

    file.ASTBuiltEvent.set();
}

async::Task<feature::CodeCompletionResult> Scheduler::codeCompletion(llvm::StringRef path,
                                                                     llvm::StringRef content,
                                                                     std::uint32_t line,
                                                                     std::uint32_t column) {
    /// Wait for PCH building.
    auto& openFile = files[path];
    if(!openFile.PCHBuild.empty()) {
        co_await openFile.PCHBuiltEvent;
    }

    /// Set compilation params ... .
    CompilationParams params;

    co_return co_await async::submit([&] { return feature::codeCompletion(params, {}); });
}

}  // namespace clice
