#pragma once

#include "Config.h"
#include "Protocol.h"
#include "Async/Async.h"
#include "Basic/SourceConverter.h"
#include "Compiler/Command.h"
#include "Compiler/Compilation.h"
#include "Support/JSON.h"
#include "Index/SymbolIndex.h"

namespace clice {

struct TranslationUnit;

struct HeaderIndex {
    /// The index file path(not include suffix, e.g. `.sidx` and `.fidx`).
    std::string path;

    /// The hash of the symbol index.
    llvm::XXH128_hash_t symbolHash;

    /// The hash of the feature index.
    llvm::XXH128_hash_t featureHash;
};

struct Context {
    /// The include chain that introduces this context.
    uint32_t include = -1;

    /// The index information of this context.
    uint32_t index = -1;
};

struct IncludeLocation {
    /// The location of the include directive.
    uint32_t line = -1;

    /// The index of the file that includes this header.
    uint32_t include = -1;

    /// The file name of the header in the string pool. Beacuse
    /// a header may be included by multiple files, so we use
    /// a string pool to cache the file name to reduce the memory
    /// usage.
    uint32_t filename = -1;
};

struct Header {
    /// The path of the header file.
    std::string srcPath;

    /// All indices of this header.
    std::vector<HeaderIndex> indices;

    /// All header contexts of this header.
    llvm::DenseMap<TranslationUnit*, std::vector<Context>> contexts;

    /// The active translation unit and the index of the context.
    std::pair<TranslationUnit*, uint32_t> active = {nullptr, -1};
};

struct TranslationUnit {
    /// The source file path.
    std::string srcPath;

    /// The index file path(not include suffix, e.g. `.sidx` and `.fidx`).
    std::string indexPath;

    /// All headers included by this translation unit.
    llvm::DenseSet<Header*> headers;

    /// The time when this translation unit is indexed. Used to determine
    /// whether the index file is outdated.
    std::chrono::milliseconds mtime;

    /// All include locations introduced by this translation unit.
    /// Note that if a file has guard macro or pragma once, we will
    /// record it at most once.
    std::vector<IncludeLocation> locations;

    /// The version of the translation unit.
    uint32_t version = 0;
};

namespace proto {

struct IncludeLocation {
    /// The line number of the include directive.
    uint32_t line;

    /// The filename of the included header.
    std::string filename;
};

struct HeaderContext {
    /// The path of the source file.
    std::string srcFile;

    /// The path of the context file.
    std::string contextFile;

    /// The index of the context.
    uint32_t index = -1;

    /// The version of the context.
    uint32_t version = 0;
};

using HeaderContextGroups = std::vector<std::vector<HeaderContext>>;

}  // namespace proto

class IncludeGraph {
protected:
    IncludeGraph(const config::IndexOptions& options) : options(options) {}

    ~IncludeGraph();

    void load(const json::Value& json);

    json::Value dump();

    async::Task<> index(llvm::StringRef file, CompilationDatabase& database);

public:
    /// Return all header context of the given file.
    /// FIXME: The results are grouped by the index file. And a header actually
    /// may have thousands of contexts, of course, users don't want to see all
    /// of them. For each index file, we return the first 10 contexts. In the future
    /// we may add a parameter to control the number of contexts or set filter.
    proto::HeaderContextGroups contextAll(llvm::StringRef file);

    /// Return current header context of the given file.
    std::optional<proto::HeaderContext> contextCurrent(llvm::StringRef file);

    /// Switch to the given header context.
    void contextSwitch(const proto::HeaderContext& context);

    /// Resolve the header context to the include chain.
    std::vector<proto::IncludeLocation> contextResolve(const proto::HeaderContext& context);

private:
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

public:
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

private:
    std::string getIndexPath(llvm::StringRef file);

    /// Check whether the given file needs to be updated. If so,
    /// return the translation unit. Otherwise, return nullptr.
    async::Task<TranslationUnit*> check(llvm::StringRef file);

    /// Add all possible header contexts for the tu from the AST info.
    uint32_t addIncludeChain(std::vector<IncludeLocation>& locations,
                             llvm::DenseMap<clang::FileID, uint32_t>& files,
                             clang::SourceManager& SM,
                             clang::FileID fid);

    void addContexts(ASTInfo& info,
                     TranslationUnit* tu,
                     llvm::DenseMap<clang::FileID, uint32_t>& files);

    async::Task<> updateIndices(ASTInfo& info,
                                TranslationUnit* tu,
                                llvm::DenseMap<clang::FileID, uint32_t>& files);

protected:
    const config::IndexOptions& options;
    llvm::StringMap<Header*> headers;
    llvm::StringMap<TranslationUnit*> tus;
    std::vector<std::string> pathPool;
    llvm::StringMap<std::uint32_t> pathIndices;
    SourceConverter SC;
};

}  // namespace clice
