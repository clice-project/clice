#pragma once

#include <Support/ADT.h>
#include <clang/Frontend/CompilerInstance.h>

namespace clice {

// TODO:

class Preamble;

std::unique_ptr<clang::CompilerInvocation> createInvocation(StringRef filename,
                                                            StringRef content,
                                                            llvm::ArrayRef<const char*> args,
                                                            Preamble* preamble = nullptr);

std::unique_ptr<clang::CompilerInstance> createInstance(std::shared_ptr<clang::CompilerInvocation> invocation);

/// - build AST
/// - build module
/// - build preamble
/// - build CodeCompletion
class Compiler {
public:
    Compiler(  // clang::DiagnosticsEngine& engine,
        llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem> vfs = llvm::vfs::getRealFileSystem()) : vfs(vfs) {}

    /// build PreCompiledHeader.
    void buildPCH(llvm::StringRef filename, llvm::StringRef content, llvm::ArrayRef<const char*> args);

    void applyPCH(clang::CompilerInvocation& invocation,
                  llvm::StringRef filename,
                  llvm::StringRef content,
                  llvm::StringRef filepath);

    /// build PreCompiledModule.
    void buildPCM(llvm::StringRef filename, llvm::StringRef content, llvm::ArrayRef<const char*> args);

    /// build AST.
    void buildAST();

private:
    // clang::DiagnosticsEngine& engine;
    llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem> vfs;
};

}  // namespace clice
