#pragma once

#include "Config.h"
#include "Database.h"
#include "Async/Async.h"
#include "Compiler/Compilation.h"
#include "Support/JSON.h"

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

class IncludeGraph {
public:
    IncludeGraph(const config::IndexOptions& options) : options(options) {}

    ~IncludeGraph();

    async::Task<> index(llvm::StringRef file, CompilationDatabase& database);

    json::Value dump();

    void load(const json::Value& json);

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

private:
    const config::IndexOptions& options;
    llvm::StringMap<Header*> headers;
    llvm::StringMap<TranslationUnit*> tus;
    std::vector<std::string> pathPool;
    llvm::StringMap<std::uint32_t> pathIndices;
};

}  // namespace clice
