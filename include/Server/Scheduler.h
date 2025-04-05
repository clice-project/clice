#pragma once

#include "Async/Async.h"
#include "Compiler/Module.h"
#include "Compiler/Command.h"
#include "Compiler/Preamble.h"
#include "Feature/CodeCompletion.h"
#include "Feature/SignatureHelp.h"

namespace clice {

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
    enum class TaskKind : std::uint8_t {
        Preprocess,
        BuildPCM,
        BuildPCH,
        Completion,
        BuildAST,
        ConsumeAST,
        BackgroundIndex,
    };

    struct Task {
        TaskKind kind;
        async::Task<> handle;
    };

    CompilationDatabase database;

    std::vector<Task> tasks;

    /// All built PCHs.
    llvm::StringMap<PCHInfo> PCHs;

    /// All built PCMs.
    llvm::StringMap<PCMInfo> PCMs;
};

}  // namespace clice
