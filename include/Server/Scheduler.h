#pragma once

#include "Indexer.h"
#include "Async/Async.h"
#include "Compiler/CompilationUnit.h"
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
    std::optional<PCHInfo> pch;
    async::Task<> pch_build_task;
    async::Event pch_built_event;

    /// For each opened file, we would like to build an AST for it.
    std::shared_ptr<CompilationUnit> ast;
    async::Task<> ast_build_task;
    async::Lock ast_built_lock;

    /// Collect all diagnostics in the compilation.
    std::shared_ptr<std::vector<Diagnostic>> diagnostics =
        std::make_unique<std::vector<Diagnostic>>();

    /// For header with context, it may have multiple ASTs, use
    /// an chain to store them.
    std::unique_ptr<OpenFile> next;
};

class Scheduler {
public:
    Scheduler(Indexer& indexer, LSPConverter& converter, CompilationDatabase& database) :
        indexer(indexer), converter(converter), database(database) {}

    /// Add or update a document.
    async::Task<OpenFile*> add_document(std::string path, std::string content);

    /// Close a document.
    void close_document(std::string path);

    llvm::StringRef getDocumentContent(llvm::StringRef path);

    /// Get the specific AST of given file.
    async::Task<json::Value> semantic_tokens(std::string path);

    async::Task<json::Value> completion(std::string path, std::uint32_t offset);

private:
    async::Task<bool> isPCHOutdated(llvm::StringRef file, llvm::StringRef preamble);

    async::Task<> build_pch(std::string file, std::string preamble);

    async::Task<> build_ast(std::string file, std::string content);

private:
    Indexer& indexer;
    LSPConverter& converter;
    CompilationDatabase& database;

    /// The task that runs in the thread pool. The number of tasks is fixed,
    /// and we won't attempt to expand the vector, so the references are
    /// guaranteed to remain valid.
    std::vector<async::Task<>> running;

    llvm::StringMap<OpenFile> opening_files;
};

}  // namespace clice
