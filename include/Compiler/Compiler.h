#pragma once

#include <Compiler/Clang.h>
#include <Compiler/Resolver.h>
#include <Support/Error.h>

namespace clice {

/// All information about AST.
struct ASTInfo {
    std::unique_ptr<clang::ASTFrontendAction> action;
    std::unique_ptr<clang::CompilerInstance> instance;
    std::unique_ptr<clang::syntax::TokenBuffer> tokBuf_;
    std::unique_ptr<TemplateResolver> resolver_;

    ASTInfo() = default;

    ASTInfo(std::unique_ptr<clang::ASTFrontendAction> action,
            std::unique_ptr<clang::CompilerInstance> instance,
            std::unique_ptr<clang::syntax::TokenBuffer> tokBuf) :
        action(std::move(action)), instance(std::move(instance)), tokBuf_(std::move(tokBuf)) {
        resolver_ = std::make_unique<TemplateResolver>(instance->getSema());
    }

    ASTInfo(const ASTInfo&) = delete;

    ASTInfo(ASTInfo&&) = default;
    ASTInfo& operator= (ASTInfo&&) = default;

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

    clang::syntax::TokenBuffer& tokBuf() {
        assert(tokBuf_ && "Token buffer is not available");
        return *tokBuf_;
    }

    TemplateResolver& resolver() {
        return *resolver_;
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
        ASTInfo(std::move(info)), path(path), mainpath(mainpath) {

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

struct PCMInfo : ASTInfo {
    /// PCM file path.
    std::string path;
    /// Module name.
    std::string name;

    PCMInfo(ASTInfo info, llvm::StringRef path) : ASTInfo(std::move(info)), path(path) {
        name = context().getCurrentNamedModule()->Name;
    }
};

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

struct CompliationParams {
    llvm::StringRef path;
    llvm::StringRef content;
    llvm::StringRef outpath;
    llvm::StringRef mainpath;
    llvm::ArrayRef<const char*> args;

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
llvm::Expected<ASTInfo> buildAST(CompliationParams& params);

llvm::Expected<PCHInfo> buildPCH(CompliationParams& params);

llvm::Expected<PCMInfo> buildPCM(CompliationParams& params);

llvm::Expected<ASTInfo> codeCompleteAt(CompliationParams& params,
                                       uint32_t line,
                                       uint32_t column,
                                       llvm::StringRef file,
                                       clang::CodeCompleteConsumer* consumer);

}  // namespace clice
