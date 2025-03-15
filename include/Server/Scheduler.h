#pragma once

#include "Async/Async.h"
#include "Compiler/Command.h"
#include "Feature/CodeCompletion.h"
#include "Feature/SignatureHelp.h"

namespace clice {

class ASTInfo;

class Scheduler {
public:
    /// Build the given source file with given content. If there is not
    /// preamble for it, build the preamble first.
    async::Task<ASTInfo> build(llvm::StringRef file, llvm::StringRef content);

    /// Build the given source directly without preamble.
    async::Task<ASTInfo> build(llvm::StringRef file);

    async::Task<feature::CodeCompletionResult> codeCompletion(llvm::StringRef file,
                                                              llvm::StringRef content,
                                                              std::uint32_t line,
                                                              std::uint32_t column);

    async::Task<feature::SignatureHelpResult> signatureHelp(llvm::StringRef file,
                                                            llvm::StringRef content,
                                                            std::uint32_t line,
                                                            std::uint32_t column);

private:
    CompilationDatabase database;
};

}  // namespace clice
