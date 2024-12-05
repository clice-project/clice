#pragma once

#include "Support/Support.h"
#include "clang/Tooling/CompilationDatabase.h"

namespace clice {

class CommandManager {
public:
    void update(llvm::StringRef dir);

    std::vector<std::string> lookup(llvm::StringRef file);
private:
    llvm::StringMap<std::unique_ptr<clang::tooling::CompilationDatabase>> CDBs;
};

}  // namespace clice
