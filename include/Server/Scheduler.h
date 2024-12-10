#pragma once

#include <deque>

#include "Server/Async.h"
#include "Server/Command.h"
#include "Server/Protocol.h"
#include "Server/Trace.h"
#include "Compiler/Compiler.h"

namespace clice {

/// A C++ source file may have different AST in different context.
/// `SourceContext` is used to distinguish different context.
struct SourceContext {
    /// Compile options for this context.
    std::string command;

    /// For a header file, it may be not self contained and need a main file.
    /// `includes` record the include chain of the header file. Each different
    /// include chain will have a different context.
    std::vector<proto::TextDocumentPositionParams> includes;
};

}  // namespace clice

template <>
struct llvm::DenseMapInfo<clice::SourceContext> {
    static clice::SourceContext getEmptyKey() {
        return clice::SourceContext{.command = "Empty", .includes = {}};
    }

    static clice::SourceContext getTombstoneKey() {
        return clice::SourceContext{.command = "Tombstone", .includes = {}};
    }

    static unsigned getHashValue(const clice::SourceContext& context) {
        return clice::refl::hash(context);
    }

    static bool isEqual(const clice::SourceContext& lhs, const clice::SourceContext& rhs) {
        return clice::refl::equal(lhs, rhs);
    }
};

namespace clice {

struct File2 {
    bool isIdle = true;

    llvm::DenseMap<SourceContext, ASTInfo> contexts;
};

struct File;

struct Task {
    /// Whether this task is a build task.
    bool isBuild = false;
    /// The coroutine handle of this task.
    std::coroutine_handle<> waiting;
};

struct File {
    bool isIdle = true;
    std::string content;
    /// The compiler instance of this file.
    ASTInfo compiler;

    std::deque<Task> waitings;
};

class Scheduler {
private:
    async::promise<void> updatePCH(llvm::StringRef path,
                                   llvm::StringRef content,
                                   llvm::StringRef command);

    async::promise<void> updatePCM() {
        co_return;
    }

    async::promise<void> buildAST(llvm::StringRef path, llvm::StringRef content);

public:
    async::promise<void> add(llvm::StringRef path, llvm::StringRef content);

    async::promise<void> update(llvm::StringRef path, llvm::StringRef content);

    async::promise<void> save(llvm::StringRef path);

    async::promise<void> close(llvm::StringRef path);

    async::promise<proto::CompletionResult> codeComplete(llvm::StringRef path,
                                                         unsigned int line,
                                                         unsigned int column);

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
    CommandManager cmdMgr;
};

}  // namespace clice
