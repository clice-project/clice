#pragma once

#include <deque>

#include "Async.h"
#include "llvm/ADT/StringMap.h"
#include "Compiler/Compiler.h"

namespace clice {

class ASTInfo;

struct File;

struct Task {
    /// Whether this task is a build task.
    bool isBuild = false;
    /// The coroutine handle of this task.
    std::coroutine_handle<> waiting;
};

struct File {
    bool isIdle = true;

    /// The compiler instance of this file.
    ASTInfo compiler;

    std::deque<Task> waitings;
};

class Scheduler {
public:
    async::promise<void> updatePCH(llvm::StringRef path,
                                   llvm::StringRef content,
                                   llvm::ArrayRef<const char*> args);

    async::promise<void> updatePCM() {
        co_return;
    }

    async::promise<void> buildAST(llvm::StringRef path, llvm::StringRef content);

    async::promise<void> add(llvm::StringRef path, llvm::StringRef content);

    async::promise<void> update(llvm::StringRef path, llvm::StringRef content);

    async::promise<void> save(llvm::StringRef path);

    async::promise<void> close(llvm::StringRef path);

    /// Schedule a task for a file. If the file is building, the task will be
    /// appended to the task list of the file and wait for the building to finish.
    /// Otherwise, the task will be executed immediately.
    template <typename Task>
    auto schedule(llvm::StringRef path, Task&& task)
        -> async::promise<decltype(task(std::declval<ASTInfo&>()))> {
        auto& file = files[path];
        if(!file.isIdle) {
            co_await async::suspend([&](auto handle) {
                file.waitings.push_back({.isBuild = false, .waiting = handle});
            });
        }

        file.isIdle = false;
        auto& compiler = file.compiler;

        auto result = co_await async::schedule_task([&task, &compiler] { return task(compiler); });

        if(!file.waitings.empty()) {
            auto task = std::move(file.waitings.front());
            async::schedule(task.waiting);
            file.waitings.pop_front();
        }

        file.isIdle = true;

        co_return result;
    }

private:
    llvm::StringMap<PCHInfo> pchs;
    llvm::StringMap<File> files;
};

}  // namespace clice
