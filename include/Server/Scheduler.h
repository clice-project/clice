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

/// Responsible for manage the files and schedule the tasks.
class Scheduler {
private:
    async::promise<> updatePCH(CompilationParams& params, class Synchronizer& sync);

    /// Clang requires all direct and indirect dependent modules to be added during module building.
    /// This function adds the dependencies of the given module to the compilation parameters.
    /// Note: It is assumed that all dependent modules have already been built.
    llvm::Error addModuleDeps(CompilationParams& params, ModuleInfo& moduleInfo);

    async::promise<> updatePCM(llvm::StringRef name, class Synchronizer& sync);

public:
    async::promise<> update(llvm::StringRef filename,
                            llvm::StringRef content,
                            class Synchronizer& sync);

    struct File {};

private:
    llvm::StringMap<PCHInfo> pchs;
    llvm::StringMap<PCMInfo> pcms;
};

}  // namespace clice
