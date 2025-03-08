#pragma once

#include "Config.h"
#include "IncludeGraph.h"
#include "Async/Async.h"
#include "Compiler/Command.h"
#include "Index/FeatureIndex.h"
#include "llvm/ADT/StringSet.h"

namespace clice {

namespace proto {

struct IncludeLocation {
    /// The line number of the include directive.
    uint32_t line;

    /// The filename of the included header.
    std::string filename;
};

/// Represent a header context,
struct HeaderContext {
    /// The path of the context file.
    std::string file;

    /// The version of the tu, used to distinguish whether
    /// the tu has updated and this context is outdated.
    uint32_t version = 0;

    /// The location index in corresponding tu's
    /// all include locations.
    uint32_t include = -1;
};

using HeaderContextGroups = std::vector<std::vector<HeaderContext>>;

}  // namespace proto

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

public:
    /// Return current header context of given header file. If the header
    /// does't have an active context, the result will be invalid.
    /// HeaderContext currentContext(llvm::StringRef header);

    /// Switch the context of the header to given context. If success,
    /// return true.
    /// bool switchContext(llvm::StringRef header, HeaderContext context);

public:
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

