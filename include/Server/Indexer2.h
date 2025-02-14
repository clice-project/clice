#pragma once

#include "Config.h"
#include "Database.h"
#include "Async/Async.h"
#include "llvm/ADT/StringSet.h"

namespace clice {

class IncludeGraph;

class Indexer2 {
public:
    Indexer2(CompilationDatabase& database, const config::IndexOptions& options);

    ~Indexer2();

    /// Add a file to wait for indexing.
    void add(std::string file);

    /// Remove a file from indexing.
    void remove(std::string file);

    void indexAll();

private:
    async::Task<> index(std::string file);

private:
    CompilationDatabase& database;
    const config::IndexOptions& options;

    llvm::StringMap<async::Task<>> tasks;
    llvm::StringSet<> pending;

    IncludeGraph* graph = nullptr;

    std::size_t concurrency = std::thread::hardware_concurrency();
};
}  // namespace clice

