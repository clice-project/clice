#pragma once

#include "Support/JSON.h"
#include "llvm/ADT/StringMap.h"

#include <expected>

namespace clice {

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

struct CompileCommand {
    /// Absolute path of the file.
    std::string file;

    /// The compile command.
    std::string command;
};

/// Try extract compile command from an item in CDB file. An reason will be returned if failed.
std::expected<CompileCommand, std::string> parseCompileCommand(const json::Object* object);

}  // namespace clice
