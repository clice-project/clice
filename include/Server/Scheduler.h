#pragma once

#include <deque>

#include "Server/Async.h"
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

struct File {
    bool isIdle = true;

    /// The current ASTInfo.
    std::unique_ptr<ASTInfo> info = std::make_unique<ASTInfo>();

    /// Coroutine handles that are waiting for the file to be updated.
    std::deque<std::coroutine_handle<>> waiters;
};

/// Responsible for manage the files and schedule the tasks.
class Scheduler {
private:
    /// Update the PCH for the given source file.
    async::promise<> updatePCH(llvm::StringRef srcPath, llvm::StringRef content);

    /// Clang requires all direct and indirect dependent modules to be added during module building.
    /// This function adds the dependencies of the given module to the compilation parameters.
    /// Note: It is assumed that all dependent modules have already been built.
    llvm::Error addModuleDeps(CompilationParams& params, const ModuleInfo& moduleInfo) const;

    /// Update the PCM for the given module.
    async::promise<> updatePCM(llvm::StringRef moduleName, class Synchronizer& sync);

    /// Update the AST for the given source file.
    async::promise<> updateAST(llvm::StringRef filename,
                               llvm::StringRef content,
                               class Synchronizer& sync);

    /// Wait for until the file is idle.
    async::promise<> waitForFile(llvm::StringRef filename);

    void scheduleNext(llvm::StringRef filename);

public:
    /// Update the given file.
    async::promise<> update(llvm::StringRef filename,
                            llvm::StringRef content,
                            class Synchronizer& sync);

    /// Execute the given action on the given file.
    async::promise<> execute(llvm::StringRef filename,
                             llvm::unique_function<void(ASTInfo&)> action);

    /// Load all Information about PCHs and PCMs from disk.
    void loadCache();

    /// Save all Information about PCHs and PCMs to disk.
    /// So that we can reuse them next time.
    void saveCache() const;

private:
    /// [file name] -> [PCHInfo]
    llvm::StringMap<PCHInfo> pchs;

    /// [module name] -> [PCMInfo]
    llvm::StringMap<PCMInfo> pcms;

    /// [file name] -> [File]
    llvm::StringMap<File> files;
};

}  // namespace clice
