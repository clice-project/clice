#pragma once

#include <expected>
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/Support/Allocator.h"

namespace clice {

/// `CompilationDatabase` is responsible for managing the compile commands.
///
/// FIXME: currently we assume that a file only occurs once in the CDB.
/// This is not always correct, but it is enough for now.
class CompilationDatabase {
public:
    using Self = CompilationDatabase;

    /// Update the compile commands with the given file.
    void update_commands(this Self& self, llvm::StringRef file);

    /// Update the module map with the given file and module name.
    void update_module(llvm::StringRef file, llvm::StringRef name);

    /// Lookup the module interface unit file path of the given module name.
    llvm::StringRef get_module_file(llvm::StringRef name);

    auto size() const {
        return commands.size();
    }

    enum class Style {
        GNU = 0,
        MSVC,
    };

    void add_command(this Self& self,
                     llvm::StringRef path,
                     llvm::StringRef command,
                     Style style = Style::GNU);

    llvm::ArrayRef<const char*> get_command(this Self& self, llvm::StringRef path);

private:
    /// Save a string into memory pool. Make sure end with `\0`.
    llvm::StringRef save_string(this Self& self, llvm::StringRef string);

    std::vector<const char*> save_args(this Self& self, llvm::ArrayRef<const char*> args);

private:
    /// For C++20 module, we only can got dependent module name
    /// in source context. But we need dependent module file path
    /// to build PCM. So we will scan(preprocess) all project files
    /// to build a module map between module name and module file path.
    /// **Note that** this only includes module interface unit, for module
    /// implementation unit, the scan could be delayed until compiling it.
    llvm::StringMap<std::string> moduleMap;

    /// Memory pool for command arguments.
    llvm::BumpPtrAllocator memory_pool;

    /// For lookup whether we already have the key.
    llvm::DenseSet<llvm::StringRef> unique;

    // A map between file path and compile commands.
    /// TODO: Path cannot represent unique file, we should use better, like inode ...
    llvm::DenseMap<const char*, std::unique_ptr<std::vector<const char*>>> commands;
};

}  // namespace clice

