#pragma once

#include <Compiler/Clang.h>

namespace clice {

class Compiler {
public:
    Compiler(llvm::StringRef filepath,
             llvm::StringRef content,
             llvm::ArrayRef<const char*> args,
             clang::DiagnosticConsumer* consumer = nullptr,
             llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem> vfs = llvm::vfs::getRealFileSystem());

    Compiler(llvm::ArrayRef<const char*> args,
             clang::DiagnosticConsumer* consumer = nullptr,
             llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem> vfs = llvm::vfs::getRealFileSystem()) :
        Compiler("", "", args, consumer, vfs) {}

    ~Compiler();

    /// Is success, return true.
    bool applyPCH(llvm::StringRef filepath, std::uint32_t bound, bool endAtStart = false);

    bool applyPCM(llvm::StringRef filepath, llvm::StringRef name);

    /// Build AST.
    void buildAST();

    /// Generate the PCH(PreCompiledHeader) to output path. Generally execute
    /// `clang::GeneratePCHAction`. The Header part of the source file is stored in the PCH file.
    /// Bound is the size of the header part.
    void generatePCH(llvm::StringRef outpath, std::uint32_t bound, bool endAtStart = false);

    /// Generate the PCM(PreCompiledModule) to output path. Generally execute
    /// `clang::GenerateReducedModuleInterfaceAction`.
    void generatePCM(llvm::StringRef outpath);

    /// Run code complete in given file and location.
    void codeCompletion(llvm::StringRef filepath, std::uint32_t line, std::uint32_t column);

    clang::Preprocessor& pp() {
        return instance->getPreprocessor();
    }

    clang::Sema& sema() {
        return instance->getSema();
    }

    clang::FileManager& fileMgr() {
        return instance->getFileManager();
    }

    clang::SourceManager& srcMgr() {
        return instance->getSourceManager();
    }

    clang::ASTContext& context() {
        return instance->getASTContext();
    }

    clang::TranslationUnitDecl* tu() {
        return instance->getASTContext().getTranslationUnitDecl();
    }

    clang::syntax::TokenBuffer& tokBuf() {
        return *buffer;
    }

private:
    void ExecuteAction();

private:
    std::string filepath;
    std::string content;
    std::unique_ptr<clang::FrontendAction> action;
    std::unique_ptr<clang::CompilerInstance> instance;
    std::unique_ptr<clang::syntax::TokenBuffer> buffer;
};

}  // namespace clice
