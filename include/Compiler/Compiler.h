#pragma once

#include <Compiler/Clang.h>
#include <Compiler/Directive.h>
#include <Compiler/Resolver.h>

#include <Support/Error.h>

namespace clice {

/// All information about AST.
class ASTInfo {
public:
    ASTInfo() = default;

    ASTInfo(std::unique_ptr<clang::ASTFrontendAction> action,
            std::unique_ptr<clang::CompilerInstance> instance,
            std::unique_ptr<clang::syntax::TokenBuffer> tokBuf,
            llvm::DenseMap<clang::FileID, Directive>&& directives) :
        action(std::move(action)), instance(std::move(instance)), m_TokBuf(std::move(tokBuf)),
        m_Directives(std::move(directives)) {
        m_Resolver = std::make_unique<TemplateResolver>(this->instance->getSema());
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
        assert(m_TokBuf && "Token buffer is not available");
        return *m_TokBuf;
    }

    TemplateResolver& resolver() {
        return *m_Resolver;
    }

    auto& directives() {
        return m_Directives;
    }

    Directive& directive(clang::FileID id) {
        return m_Directives[id];
    }

    /// Get the length of the token at the given location.
    auto getTokenLength(clang::SourceLocation loc) {
        return clang::Lexer::MeasureTokenLength(loc, srcMgr(), instance->getLangOpts());
    }

    /// Get the spelling of the token at the given location.
    llvm::StringRef getTokenSpelling(clang::SourceLocation loc) {
        return llvm::StringRef(srcMgr().getCharacterData(loc), getTokenLength(loc));
    }

    auto getLocation(clang::SourceLocation loc) {
        return srcMgr().getPresumedLoc(loc);
    }

private:
    std::unique_ptr<clang::ASTFrontendAction> action;
    std::unique_ptr<clang::CompilerInstance> instance;
    std::unique_ptr<clang::syntax::TokenBuffer> m_TokBuf;
    std::unique_ptr<TemplateResolver> m_Resolver;
    llvm::DenseMap<clang::FileID, Directive> m_Directives;
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

    llvm::IntrusiveRefCntPtr<vfs::FileSystem> vfs = new ThreadSafeFS();

    uint32_t line = 0, column = 0;

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
llvm::Expected<ASTInfo> compile(CompliationParams& params);

llvm::Expected<ASTInfo> compile(CompliationParams& params, PCHInfo& out);

llvm::Expected<ASTInfo> compile(CompliationParams& params, PCMInfo& out);

llvm::Expected<ASTInfo> compile(CompliationParams& params, clang::CodeCompleteConsumer* consumer);

}  // namespace clice
