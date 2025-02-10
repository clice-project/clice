#pragma once

#include "Config.h"
#include "Database.h"
#include "Protocol.h"
#include "Async/Async.h"
#include "AST/RelationKind.h"
#include "Index/SymbolIndex.h"
#include "llvm/ADT/DenseSet.h"

namespace clice {

class ASTInfo;

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
};

class Indexer {
public:
    Indexer(const config::IndexOptions& options, CompilationDatabase& database) :
        options(options), database(database) {}

    ~Indexer();

    using Self = Indexer;

private:
    /// Check whether the given file needs to be updated and return the translation unit.
    /// If not need to update, return nullptr.
    async::Task<TranslationUnit*> check(this Self& self, llvm::StringRef file);

    uint32_t addIncludeChain(std::vector<IncludeLocation>& locations,
                             llvm::DenseMap<clang::FileID, uint32_t>& files,
                             clang::SourceManager& SM,
                             clang::FileID fid);

    /// Add all possible header contexts for the AST info.
    void addContexts(this Self& self,
                     ASTInfo& info,
                     TranslationUnit* tu,
                     llvm::DenseMap<clang::FileID, uint32_t>& files);

    /// Index the given AST, write the index information to disk.
    async::Task<> updateIndices(this Self& self,
                                ASTInfo& info,
                                TranslationUnit* tu,
                                llvm::DenseMap<clang::FileID, uint32_t>& files);

    struct SymbolID {
        uint64_t id;
        std::string name;
    };

    /// Generate the index file path based on time and file name.
    std::string getIndexPath(llvm::StringRef file);

public:
    async::Task<> index(this Self& self, llvm::StringRef file);

    async::Task<> index(llvm::StringRef file, ASTInfo& info);

    async::Task<> indexAll();

    /// FIXME: Add header context here.
    using LookupCallback =
        llvm::unique_function<void(llvm::StringRef path, const index::SymbolIndex::Symbol&)>;

    struct HeaderContext {
        llvm::StringRef srcFile;
        llvm::StringRef contextFile;
        uint32_t include;
    };

    std::string getIndexPath(const HeaderContext& context);

    async::Task<> lookup(const HeaderContext& context, uint32_t offset, LookupCallback callback);

    async::Task<> lookup(const HeaderContext& context,
                         uint64_t id,
                         llvm::StringRef name,
                         LookupCallback callback);

    async::Task<> lookup(llvm::StringRef file, uint32_t offset, LookupCallback callback);

    /// This method make sures all path in lookup callback is same sa input file.
    async::Task<> resolve(llvm::StringRef file, uint32_t offset, LookupCallback callback);

    async::Task<proto::SemanticTokens> semanticTokens(llvm::StringRef file);

    /// Dump the index information to JSON.
    json::Value dumpToJSON();

    /// Dump all index information of the given file for test.
    void dumpForTest(llvm::StringRef file);

    /// Save the index information to disk.
    void saveToDisk();

    /// Load the index information from disk.
    void loadFromDisk();

private:
    const config::IndexOptions& options;
    CompilationDatabase& database;
    llvm::StringMap<Header*> headers;
    llvm::StringMap<TranslationUnit*> tus;

    bool locked = false;

    std::vector<std::string> pathPool;
    llvm::StringMap<std::uint32_t> pathIndices;

    llvm::StringMap<std::string> featureCache;
};

}  // namespace clice
