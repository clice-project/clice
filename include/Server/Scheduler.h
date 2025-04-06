#pragma once

#include "Async/Async.h"
#include "Compiler/AST.h"
#include "Compiler/Module.h"
#include "Compiler/Command.h"
#include "Compiler/Preamble.h"
#include "Feature/CodeCompletion.h"
#include "Feature/SignatureHelp.h"

namespace clice {

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
    std::shared_ptr<ASTInfo> AST;
    async::Task<> ASTBuild;
    async::Event ASTBuiltEvent;

    /// For header with context, it may have multiple ASTs, use
    /// an chain to store them.
    std::unique_ptr<OpenFile> next;
};

class Scheduler {
public:
    Scheduler(CompilationDatabase& database) : database(database) {}

    /// Add or update a document.
    void addDocument(std::string path, std::string content);

    /// Close a document.
    void closeDocument(std::string path);

private:
    async::Task<bool> isPCHOutdated(llvm::StringRef file, llvm::StringRef preamble);

    async::Task<> buildPCH(std::string file, std::string preamble);

    async::Task<> buildAST(std::string file, std::string content);

    async::Task<feature::CodeCompletionResult> codeCompletion(llvm::StringRef file,
                                                              llvm::StringRef content,
                                                              std::uint32_t line,
                                                              std::uint32_t column);

    async::Task<feature::SignatureHelpResult> signatureHelp(llvm::StringRef file,
                                                            llvm::StringRef content,
                                                            std::uint32_t line,
                                                            std::uint32_t column);

private:
    CompilationDatabase& database;

    /// The task that runs in the thread pool. The number of tasks is fixed,
    /// and we won't attempt to expand the vector, so the references are
    /// guaranteed to remain valid.
    std::vector<async::Task<>> running;

    llvm::StringMap<OpenFile> openFiles;
};

}  // namespace clice
