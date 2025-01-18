#pragma once

#include "Async.h"
#include "Database.h"

namespace clice {

class ASTInfo;

class Indexer {
public:
    Indexer(CompilationDatabase& database) : database(database) {}

    /// Index the given file(for unopened file).
    async::Task<> index(llvm::StringRef file);

    /// Index the given file(for opened file).
    async::Task<> index(llvm::StringRef file, ASTInfo& info);

private:
    CompilationDatabase& database;
};

}  // namespace clice
