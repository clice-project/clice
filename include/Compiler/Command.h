#pragma once

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/Support/Allocator.h"
#include "clang/Tooling/CompilationDatabase.h"

namespace clice {

class CommandManager {
public:
    /// FIXME: support multiple compilation databases.
    CommandManager(llvm::StringRef path);

    llvm::ArrayRef<const char*> lookup(llvm::StringRef file);

    // TODO: add a function for scaning module dependencies.

    struct Data {
        std::uint64_t index;
        std::uint64_t size;
    };

private:
    std::vector<const char*> args;
    llvm::BumpPtrAllocator allocator;

    llvm::StringMap<Data> cache;

    /// module name -> file path
    llvm::StringMap<std::string> moduleMap;

    std::unique_ptr<clang::tooling::CompilationDatabase> CDB;
};

}  // namespace clice
