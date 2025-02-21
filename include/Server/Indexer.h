#pragma once

#include "Config.h"
#include "Database.h"
#include "Async/Async.h"
#include "llvm/ADT/StringSet.h"
#include "IncludeGraph.h"

namespace clice {

class IncludeGraph;

class Indexer : public IncludeGraph {
public:
    Indexer(CompilationDatabase& database, const config::IndexOptions& options);

    /// Add a file to wait for indexing.
    void add(std::string file);

    /// Remove a file from indexing.
    void remove(std::string file);

    void indexAll();

    void save();

    void load();

private:
    async::Task<> index(std::string file);

private:
    CompilationDatabase& database;
    const config::IndexOptions& options;

    llvm::StringMap<async::Task<>> tasks;
    llvm::StringSet<> pending;

    std::size_t concurrency = std::thread::hardware_concurrency();
};
}  // namespace clice

