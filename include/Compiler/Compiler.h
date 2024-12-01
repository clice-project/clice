#pragma once

#include <Compiler/Clang.h>
#include <Compiler/Resolver.h>
#include <Support/Error.h>

namespace clice {

/// All information about AST.
class ASTInfo {
public:
    ASTInfo() = default;

    ASTInfo(std::unique_ptr<clang::ASTFrontendAction> action,
            std::unique_ptr<clang::CompilerInstance> instance,
            std::unique_ptr<clang::syntax::TokenBuffer> tokBuf) :
        action(std::move(action)), instance(std::move(instance)), tokBuf_(std::move(tokBuf)) {
        resolver_ = std::make_unique<TemplateResolver>(this->instance->getSema());
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

private:
    std::unique_ptr<clang::ASTFrontendAction> action;
    std::unique_ptr<clang::CompilerInstance> instance;
    std::unique_ptr<clang::syntax::TokenBuffer> tokBuf_;
    std::unique_ptr<TemplateResolver> resolver_;
};

struct PCHInfo {
    /// PCM file path.
    std::string path;
    /// Source file path.
    std::string mainpath;
    /// The content of source file used to build this PCM.
    std::string preamble;
    /// Files involved in building this PCM.
    std::vector<std::string> deps;

    clang::PreambleBounds bounds() const {
        /// We use '@' to mark the end of the preamble.
        bool endAtStart = preamble.ends_with('@');
        unsigned int size = preamble.size() - endAtStart;
        return {size, endAtStart};
    }

    bool needUpdate(llvm::StringRef content);
};

struct PCMInfo {
    /// PCM file path.
    std::string path;
    /// Module name.
    std::string name;

    bool needUpdate();
};

struct CompliationParams {
    llvm::StringRef content;
    llvm::SmallString<128> path;
    llvm::SmallString<128> outpath;
    llvm::SmallString<128> mainpath;
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

    void addPCM(const PCMInfo& info) {
        pcms.emplace_back(info.name, info.path);
    }
};

/// Build AST from given file path and content. If pch or pcm provided, apply them to the compiler.
/// Note this function will not check whether we need to update the PCH or PCM, caller should check
/// their reusability and update in time.
llvm::Expected<ASTInfo> buildAST(CompliationParams& params);

llvm::Expected<ASTInfo> buildPCH(CompliationParams& params, PCHInfo& out);

llvm::Expected<ASTInfo> buildPCM(CompliationParams& params, PCMInfo& out);

llvm::Expected<ASTInfo> codeCompleteAt(CompliationParams& params,
                                       uint32_t line,
                                       uint32_t column,
                                       llvm::StringRef file,
                                       clang::CodeCompleteConsumer* consumer);

}  // namespace clice
