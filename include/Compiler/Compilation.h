#pragma once

#include "Module.h"
#include "Preamble.h"
#include "CompilationUnit.h"
#include "Support/FileSystem.h"

namespace clang {
class CodeCompleteConsumer;
}

namespace clice {

struct CompilationParams {
    /// Source file content.
    llvm::StringRef content;

    /// Source file path.
    llvm::SmallString<128> srcPath;

    /// Output file path.
    llvm::SmallString<128> outPath;

    /// Responsible for storing the arguments.
    llvm::SmallString<1024> command;

    /// - If we are building PCH, we need a size to verify the bounds of preamble. That is
    /// which source code range the PCH will cover.
    /// - If we are building main file AST for header, we need a size to cut off code after the
    /// `#include` directive that includes the header to speed up the parsing.
    std::optional<std::uint32_t> bound;

    llvm::IntrusiveRefCntPtr<vfs::FileSystem> vfs = new ThreadSafeFS();

    /// Information about reuse PCH.
    std::pair<std::string, uint32_t> pch;

    /// Information about reuse PCM(name, path).
    llvm::StringMap<std::string> pcms;

    /// Code completion file:offset.
    std::tuple<std::string, std::uint32_t> completion;

    /// The memory buffers for all remapped file.
    llvm::StringMap<std::unique_ptr<llvm::MemoryBuffer>> buffers;

    void addRemappedFile(llvm::StringRef path, llvm::StringRef content) {
        buffers.try_emplace(path, llvm::MemoryBuffer::getMemBufferCopy(content));
    }
};

using CompilationResult = std::expected<CompilationUnit, std::string>;

/// Only preprocess ths source flie.
CompilationResult preprocess(CompilationParams& params);

/// Build AST from given file path and content. If pch or pcm provided, apply them to the compiler.
/// Note this function will not check whether we need to update the PCH or PCM, caller should check
/// their reusability and update in time.
CompilationResult compile(CompilationParams& params);

/// Build PCH from given file path and content.
CompilationResult compile(CompilationParams& params, PCHInfo& out);

/// Build PCM from given file path and content.
CompilationResult compile(CompilationParams& params, PCMInfo& out);

/// Run code completion at the given location.
CompilationResult complete(CompilationParams& params, clang::CodeCompleteConsumer* consumer);

}  // namespace clice
