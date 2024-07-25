#pragma once

#include "Clang.h"

namespace clice {

auto createCompilerInvocation(clang::tooling::CompileCommand& command,
                              std::vector<std::string>* extraArgs,
                              llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem> vfs,
                              clang::DiagnosticConsumer& consumer) -> std::unique_ptr<CompilerInvocation>;

auto createCompilerInstance(std::unique_ptr<CompilerInvocation> invocation,
                            const clang::PrecompiledPreamble* preamble,
                            std::unique_ptr<llvm::MemoryBuffer> content,
                            llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem> vfs,
                            clang::DiagnosticConsumer& consumer) -> std::unique_ptr<CompilerInstance>;

}  // namespace clice
