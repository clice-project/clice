#pragma once

#include "Config.h"
#include "IncludeGraph.h"
#include "Async/Async.h"
#include "Compiler/Command.h"
#include "Index/FeatureIndex.h"
#include "llvm/ADT/StringSet.h"

namespace clice {

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

    async::Task<std::optional<index::FeatureIndex>> getFeatureIndex(std::string& buffer,
                                                                    llvm::StringRef file) const;

    async::Task<std::vector<feature::SemanticToken>> semanticTokens(llvm::StringRef file) const;

    async::Task<std::vector<feature::FoldingRange>> foldingRanges(llvm::StringRef file) const;

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

