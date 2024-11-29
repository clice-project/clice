#pragma once

#include <Compiler/Clang.h>
#include <Compiler/Resolver.h>
#include <Support/Error.h>

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
    void codeCompletion(llvm::StringRef filepath,
                        std::uint32_t line,
                        std::uint32_t column,
                        clang::CodeCompleteConsumer* consumer);

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

    TemplateResolver& resolver() {
        return *m_Resolver;
    }

private:
    void ExecuteAction();

private:
    std::string filepath;
    std::string content;
    std::unique_ptr<clang::ASTFrontendAction> action;
    std::unique_ptr<clang::CompilerInstance> instance;
    std::unique_ptr<clang::syntax::TokenBuffer> buffer;
    std::unique_ptr<TemplateResolver> m_Resolver;
};

/// All information about AST.
struct ASTInfo {
    std::unique_ptr<clang::ASTFrontendAction> action;
    std::unique_ptr<clang::CompilerInstance> instance;
    std::unique_ptr<clang::syntax::TokenBuffer> tokBuf;

    ASTInfo(std::unique_ptr<clang::ASTFrontendAction> action,
            std::unique_ptr<clang::CompilerInstance> instance,
            std::unique_ptr<clang::syntax::TokenBuffer> tokBuf) :
        action(std::move(action)), instance(std::move(instance)), tokBuf(std::move(tokBuf)) {}

    ASTInfo(const ASTInfo&) = delete;

    ASTInfo(ASTInfo&&) = default;

    ~ASTInfo() {
        if(action) {
            action->EndSourceFile();
        }
    }

    clang::Sema& sema() {
        return instance->getSema();
    }

    clang::ASTContext& context() {
        return instance->getASTContext();
    }

    clang::SourceManager& srcMgr() {
        return instance->getSourceManager();
    }

    clang::Preprocessor& pp() {
        return instance->getPreprocessor();
    }

    clang::TranslationUnitDecl* tu() {
        return instance->getASTContext().getTranslationUnitDecl();
    }
};

struct PCHInfo : ASTInfo {
    /// PCM file path.
    std::string path;
    /// Source file path.
    std::string mainpath;
    /// The content of source file used to build this PCM.
    std::string preamble;
    /// Files involved in building this PCM.
    std::vector<std::string> deps;

    PCHInfo(ASTInfo info,
            llvm::StringRef path,
            llvm::StringRef content,
            llvm::StringRef mainpath,
            clang::PreambleBounds bounds) :
        ASTInfo(std::move(info)), path(std::move(path)), mainpath(mainpath) {

        preamble = content.substr(0, bounds.Size).str();
        if(bounds.PreambleEndsAtStartOfLine) {
            preamble.append("@");
        }
    }

    clang::PreambleBounds bounds() const {
        /// We use '@' to mark the end of the preamble.
        bool endAtStart = preamble.ends_with('@');
        unsigned int size = preamble.size() - endAtStart;
        return {size, endAtStart};
    }
};

struct PCMInfo : ASTInfo {};

/// Information about reuse PCH or PCM. This should be placed in stack.
struct Preamble {
    /// Information about reuse PCH.
    std::string pch;
    clang::PreambleBounds bounds = {0, false};

    /// Information about reuse PCM(name, path).
    llvm::SmallVector<std::pair<std::string, std::string>> pcms;

    void addPCH(const PCHInfo& info) {
        pch = info.path;
        bounds = info.bounds();
    }
};

/// Build AST from given file path and content. If pch or pcm provided, apply them to the compiler.
/// Note this function will not check whether we need to update the PCH or PCM, caller should check
/// their reusability and update in time.
llvm::Expected<ASTInfo> buildAST(llvm::StringRef path,
                                 llvm::StringRef content,
                                 llvm::ArrayRef<const char*> args,
                                 Preamble* preamble = nullptr);

llvm::Expected<PCHInfo> buildPCH(llvm::StringRef path,
                                 llvm::StringRef content,
                                 llvm::StringRef outpath,
                                 llvm::ArrayRef<const char*> args);

llvm::Expected<PCHInfo> buildPCM(llvm::StringRef path,
                                 llvm::StringRef content,
                                 llvm::StringRef outpath,
                                 llvm::ArrayRef<const char*> args);

}  // namespace clice
