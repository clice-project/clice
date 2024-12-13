#pragma once

#include "Support/Support.h"

namespace clice {

/// Responsible for synchronizing changes to the CDB file,
/// including updating compile commands and the module map.
class Synchronizer {
public:
    /// FIXME: The CDB file can be very large, reaching the size of
    /// several gigabytes (GB). Therefore, it's better for this function
    /// to be an asynchronous function.

    /// Update the compile commands.
    void sync(llvm::StringRef file);

    /// Update the module map for active file.
    void sync(llvm::StringRef name, llvm::StringRef path);

    /// Lookup the compile commands of the given file.
    llvm::StringRef lookup(llvm::StringRef file);

    /// Lookup the module interface unit file path of the given module name.
    llvm::StringRef map(llvm::StringRef name);

private:
    /// FIXME: currently we assume that a file only occurs once in the CDB.
    /// This is not always correct, but it is enough for now.

    /// A map between file path and compile commands.
    llvm::StringMap<std::string> commands;

    /// For C++20 module, we only can got dependent module name
    /// in source context. But we need dependent module file path
    /// to build PCM. So we will scan(preprocess) all project files
    /// to build a module map between module name and module file path.
    /// **Note that** this only includes module interface unit, for module
    /// implementation unit, the scan could be delayed until compiling it.
    llvm::StringMap<std::string> moduleMap;
};

}  // namespace clice
