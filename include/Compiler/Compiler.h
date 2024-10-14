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

class Compiler {
public:
    Compiler(llvm::StringRef filepath,
             llvm::StringRef content,
             llvm::ArrayRef<const char*> args,
             llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem> vfs = llvm::vfs::getRealFileSystem());

    ~Compiler();

    /// Is success, return true.
    bool applyPCH(llvm::StringRef filepath, std::uint32_t bound, bool endAtStart = false);

    bool applyPCM(llvm::StringRef filepath, llvm::StringRef name);

    /// build AST.
    void buildAST();

    /// Generate the PCH(PreCompiledHeader) to output path. Generally execute `clang::GeneratePCHAction`.
    /// The Header part of the source file is stored in the PCH file. Bound is the size of the header part.
    void generatePCH(llvm::StringRef outpath, std::uint32_t bound, bool endAtStart = false);

    /// Generate the PCM(PreCompiledModule) to output path. Generally execute
    /// `clang::GenerateReducedModuleInterfaceAction`.
    void generatePCM(llvm::StringRef outpath);

    void codeCompletion(llvm::StringRef filepath,
                        std::uint32_t line,
                        std::uint32_t column,
                        clang::CodeCompleteConsumer* consumer);

    clang::Sema& sema() {
        return instance->getSema();
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

private:
    std::string filepath;
    std::string content;
    std::unique_ptr<clang::FrontendAction> action;
    std::unique_ptr<clang::CompilerInstance> instance;
    std::unique_ptr<clang::syntax::TokenBuffer> buffer;
};

}  // namespace clice
