#pragma once

#include <expected>
#include "llvm/ADT/StringMap.h"

namespace clice {

/// Processes and adjusts a raw compile command from compile_commands.json.
///
/// This function tokenizes the input command, removes unnecessary arguments,
/// and ensures the resulting format is suitable for execution.
///
/// @param command The raw shell-escaped compile command.
/// @param out A vector to hold pointers to the processed arguments.
/// @param buffer A storage buffer for the actual argument strings.
std::expected<void, std::string> mangle_command(llvm::StringRef command,
                                               llvm::SmallVectorImpl<const char*>& out,
                                               llvm::SmallVectorImpl<char>& buffer);

/// `CompilationDatabase` is responsible for managing the compile commands.
///
/// FIXME: currently we assume that a file only occurs once in the CDB.
/// This is not always correct, but it is enough for now.
class CompilationDatabase {
public:
    /// Update the compile commands with the given file.
    void updateCommands(llvm::StringRef file);

    /// Update the compile commands with the given file and compile command.
    void updateCommand(llvm::StringRef file, llvm::StringRef command);

    /// Update the module map with the given file and module name.
    void updateModule(llvm::StringRef file, llvm::StringRef name);

    /// Lookup the compile commands of the given file.
    llvm::StringRef getCommand(llvm::StringRef file);

    /// Lookup the module interface unit file path of the given module name.
    llvm::StringRef getModuleFile(llvm::StringRef name);

    auto size() const {
        return commands.size();
    }

    auto begin() {
        return commands.begin();
    }

    auto end() {
        return commands.end();
    }

private:
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
