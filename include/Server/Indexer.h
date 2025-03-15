#pragma once

#include "Config.h"
#include "IncludeGraph.h"
#include "Async/Async.h"
#include "Compiler/Command.h"
#include "Index/FeatureIndex.h"
#include "llvm/ADT/StringSet.h"
#include "Feature/Lookup.h"

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

public:
    Header* getHeader(llvm::StringRef file) const;

    TranslationUnit* getTranslationUnit(llvm::StringRef file) const;

    /// Return current header context of given header file. If the header
    /// does't have an active context, the result will be invalid.
    std::optional<proto::HeaderContext> currentContext(llvm::StringRef header) const;

    /// Switch the context of the header to given context. If success,
    /// return true.
    bool switchContext(llvm::StringRef header, proto::HeaderContext context);

    /// Resolve the given header context to a group of locations.
    std::vector<proto::IncludeLocation> resolveContext(proto::HeaderContext context) const;

    /// Return all header contexts of given header file, note that a header may have thousands
    /// of header contexts, of course we won't return them all at once. We would return a group
    /// of contexts for each different header context. The maximum of group count is determined
    /// by limit. Optionally, you can specify a 'contextFile' to filter the results, returning only
    /// contexts related to that file.
    std::vector<proto::HeaderContextGroup>
        allContexts(llvm::StringRef headerFile,
                    uint32_t limit = 10,
                    llvm::StringRef contextFile = llvm::StringRef()) const;

public:
    struct SymbolID {
        uint64_t hash;
        std::string name;
    };

    /// Return all indices of the given translation unit. If the file is empty,
    /// return all indices of the IncludeGraph.
    std::vector<std::string> indices(TranslationUnit* tu = nullptr);

    /// Resolve the symbol at the given position.
    async::Task<std::vector<SymbolID>> resolve(const proto::TextDocumentPositionParams& params);

    using LookupCallback = llvm::unique_function<bool(llvm::StringRef path,
                                                      llvm::StringRef content,
                                                      const index::SymbolIndex::Symbol& symbol)>;

    async::Task<> lookup(llvm::ArrayRef<SymbolID> targets,
                         llvm::ArrayRef<std::string> files,
                         LookupCallback callback);

    /// Lookup the reference information according to the given position.
    async::Task<proto::ReferenceResult> lookup(const proto::ReferenceParams& params,
                                               RelationKind kind);

    /// According to the given file and offset, resolve the symbol at the offset.
    async::Task<proto::HierarchyPrepareResult>
        prepareHierarchy(const proto::HierarchyPrepareParams& params);

    async::Task<proto::CallHierarchyIncomingCallsResult>
        incomingCalls(const proto::HierarchyParams& params);

    async::Task<proto::CallHierarchyOutgoingCallsResult>
        outgoingCalls(const proto::HierarchyParams& params);

    async::Task<proto::TypeHierarchyResult> typeHierarchy(const proto::HierarchyParams& params,
                                                          bool super);

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

