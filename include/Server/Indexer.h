#pragma once

#include "Async.h"
#include "Config.h"
#include "Database.h"

namespace clice {

class ASTInfo;

class Indexer {
public:
    Indexer(const config::IndexOptions& options, CompilationDatabase& database) :
        options(options), database(database) {}

    ~Indexer();

    /// Index the given file(for unopened file).
    async::Task<> index(llvm::StringRef file);

    /// Index the given file(for opened file).
    async::Task<> index(llvm::StringRef file, ASTInfo& info);

    /// Dump the index information to JSON.
    json::Value dumpToJSON();

    /// Save the index information to disk.
    void saveToDisk();

    /// Load the index information from disk.
    void loadFromDisk();

    struct TranslationUnit;

    struct Context {
        /// The index file path(not include suffix, e.g. `.sidx` and `.fidx`).
        std::string index;

        /// The include chain that introduces this context.
        uint32_t include;
    };

    struct IncludeLocation {
        /// The location of the include directive.
        uint32_t line;

        /// The index of the file that includes this header.
        uint32_t include = -1;

        /// The file name of the header.
        std::string filename;
    };

    struct Header {
        /// The path of the header file.
        std::string srcPath;

        /// All header contexts of this header.
        llvm::DenseMap<TranslationUnit*, std::vector<Context>> contexts;
    };

    struct TranslationUnit {
        /// The source file path.
        std::string srcPath;

        /// The index file path(not include suffix, e.g. `.sidx` and `.fidx`).
        std::string index;

        /// All headers included by this translation unit.
        std::vector<Header*> headers;

        /// All include locations introduced by this translation unit.
        /// Note that if a file has guard macro or pragma once, we will
        /// record it at most once.
        std::vector<IncludeLocation> locations;
    };

private:
    const config::IndexOptions& options;
    CompilationDatabase& database;
    llvm::StringMap<Header*> headers;
    llvm::StringMap<TranslationUnit*> tus;
};

}  // namespace clice
