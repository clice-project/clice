#pragma once

#include "Support/Support.h"
#include "clang/Tooling/CompilationDatabase.h"

namespace clice {

class CommandManager {
public:
    void update(llvm::StringRef dir);

    /// Return the commands of first meet file.
    llvm::StringRef lookupFirst(llvm::StringRef file);

    llvm::ArrayRef<std::string> lookup(llvm::StringRef file);

private:
    /// CDB file -> file -> [commands]
    using Commands = std::vector<std::string>;
    using CDB = llvm::StringMap<Commands>;
    llvm::StringMap<CDB> CDBs;

    /// Module name -> module file path.
    llvm::StringMap<std::string> moduleMap;
};

}  // namespace clice
