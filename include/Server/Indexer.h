#pragma once

#include "Async.h"
#include "Config.h"
#include "Database.h"
#include "Protocol.h"
#include "Basic/RelationKind.h"

namespace clice {

class ASTInfo;

class Indexer {
public:
    Indexer(const config::IndexOptions& options, CompilationDatabase& database) :
        options(options), database(database) {}

    ~Indexer();

    struct TranslationUnit;

    struct Context {
        /// The index file path(not include suffix, e.g. `.sidx` and `.fidx`).
        std::string indexPath;

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

    /// Check whether the index file is outdated.
    async::Task<bool> needUpdate(TranslationUnit* tu);

    /// Try to merge the index file with same content for the given header.
    async::Task<> merge(Header* header);

    async::Task<> merge(llvm::StringRef header);

    async::Task<> mergeAll();

    /// Index the given file(for unopened file).
    async::Task<> index(llvm::StringRef file);

    /// Index the given file(for opened file).
    async::Task<> index(llvm::StringRef file, ASTInfo& info);

    async::Task<> indexAll();

    /// Generate the index file path based on time and file name.
    std::string getIndexPath(llvm::StringRef file);

    /// Dump the index information to JSON.
    json::Value dumpToJSON();

    /// Dump all index information of the given file for test.
    void dumpForTest(llvm::StringRef file);

    void lookupHeaderContexts(llvm::StringRef file);

    /// Save the index information to disk.
    void saveToDisk();

    /// Load the index information from disk.
    void loadFromDisk();

private:
    struct SymbolID {
        uint64_t id;
        std::string name;
    };

    async::Task<std::unique_ptr<llvm::MemoryBuffer>> read(llvm::StringRef path);

    async::Task<> lookup(llvm::ArrayRef<SymbolID> ids,
                         RelationKind kind,
                         llvm::StringRef srcPath,
                         llvm::StringRef content,
                         std::string indexPath,
                         std::vector<proto::Location>& result);

public:
    async::Task<std::vector<proto::Location>>
        lookup(const proto::TextDocumentPositionParams& params, RelationKind kind);

    async::Task<proto::CallHierarchyIncomingCallsResult>
        incomingCalls(const proto::CallHierarchyIncomingCallsParams& params);

    async::Task<proto::CallHierarchyOutgoingCallsResult>
        outgoingCalls(const proto::CallHierarchyOutgoingCallsParams& params);

    async::Task<proto::TypeHierarchySupertypesResult>
        supertypes(const proto::TypeHierarchySupertypesParams& params);

    async::Task<proto::TypeHierarchySubtypesResult>
        subtypes(const proto::TypeHierarchySubtypesParams& params);

private:
    const config::IndexOptions& options;
    CompilationDatabase& database;
    llvm::StringMap<Header*> headers;
    llvm::StringMap<TranslationUnit*> tus;
};

}  // namespace clice
