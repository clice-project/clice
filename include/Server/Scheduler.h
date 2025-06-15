#pragma once

#include "Indexer.h"
#include "Async/Async.h"
#include "Compiler/AST.h"
#include "Compiler/Module.h"
#include "Compiler/Preamble.h"

namespace clice {

class LSPConverter;
class CompilationDatabase;

struct OpenFile {
    /// The file version, every edition will increase it.
    std::uint32_t version = 0;

    /// The file content.
    std::string content;

    /// We build PCH for every opened file.
    std::optional<PCHInfo> PCH;
    async::Task<> PCHBuild;
    async::Event PCHBuiltEvent;

    /// For each opened file, we would like to build an AST for it.
    std::shared_ptr<CompilationUnit> AST;
    async::Task<> ASTBuild;
    async::Lock ASTBuiltLock;

    /// For header with context, it may have multiple ASTs, use
    /// an chain to store them.
    std::unique_ptr<OpenFile> next;
};

class Scheduler {
public:
    Scheduler(Indexer& indexer, LSPConverter& converter, CompilationDatabase& database) :
        indexer(indexer), converter(converter), database(database) {}

    /// Add or update a document.
    void addDocument(std::string path, std::string content);

    /// Close a document.
    void closeDocument(std::string path);

    llvm::StringRef getDocumentContent(llvm::StringRef path);

    /// Get the specific AST of given file.
    async::Task<json::Value> semanticToken(std::string path);

    async::Task<json::Value> completion(std::string path, std::uint32_t offset);

private:
    async::Task<bool> isPCHOutdated(llvm::StringRef file, llvm::StringRef preamble);

    async::Task<> buildPCH(std::string file, std::string preamble);

    async::Task<> buildAST(std::string file, std::string content);

private:
    Indexer& indexer;
    LSPConverter& converter;
    CompilationDatabase& database;

    /// The task that runs in the thread pool. The number of tasks is fixed,
    /// and we won't attempt to expand the vector, so the references are
    /// guaranteed to remain valid.
    std::vector<async::Task<>> running;

    llvm::StringMap<OpenFile> openFiles;
};

}  // namespace clice
