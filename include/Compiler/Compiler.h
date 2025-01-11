#pragma once

#include "Clang.h"
#include "Module.h"
#include "Preamble.h"
#include "Resolver.h"
#include "Directive.h"

#include "Support/Error.h"
#include "Basic/Location.h"

#include "llvm/ADT/StringSet.h"

namespace clice {

struct CompilationParams;

/// All information about AST.
class ASTInfo {
public:
    ASTInfo() = default;

    ASTInfo(std::unique_ptr<clang::FrontendAction> action,
            std::unique_ptr<clang::CompilerInstance> instance,
            std::unique_ptr<clang::syntax::TokenBuffer> tokBuf,
            llvm::DenseMap<clang::FileID, Directive>&& directives,
            std::vector<std::string> deps) :
        action(std::move(action)), m_Instance(std::move(instance)), m_TokBuf(std::move(tokBuf)),
        m_Directives(std::move(directives)), m_Deps(std::move(deps)) {
        m_Resolver = std::make_unique<TemplateResolver>(this->m_Instance->getSema());
    }

    ASTInfo(const ASTInfo&) = delete;

    ASTInfo(ASTInfo&&) = default;
    ASTInfo& operator= (ASTInfo&&) = default;

    ~ASTInfo() {
        if(action) {
            action->EndSourceFile();
        }
    }

    auto& sema() {
        return m_Instance->getSema();
    }

    auto& context() {
        return m_Instance->getASTContext();
    }

    auto& srcMgr() {
        return m_Instance->getSourceManager();
    }

    auto& pp() {
        return m_Instance->getPreprocessor();
    }

    clang::TranslationUnitDecl* tu() {
        return m_Instance->getASTContext().getTranslationUnitDecl();
    }

    auto& tokBuf() {
        assert(m_TokBuf && "Token buffer is not available");
        return *m_TokBuf;
    }

    auto& resolver() {
        return *m_Resolver;
    }

    auto& directives() {
        return m_Directives;
    }

    auto& directive(clang::FileID id) {
        return m_Directives[id];
    }

    auto& deps() {
        return m_Deps;
    }

    auto& instance() {
        return *m_Instance;
    }

    /// Get the length of the token at the given location.
    auto getTokenLength(clang::SourceLocation loc) {
        return clang::Lexer::MeasureTokenLength(loc, srcMgr(), m_Instance->getLangOpts());
    }

    /// Get the spelling of the token at the given location.
    llvm::StringRef getTokenSpelling(clang::SourceLocation loc) {
        return llvm::StringRef(srcMgr().getCharacterData(loc), getTokenLength(loc));
    }

    auto getLocation(clang::SourceLocation loc) {
        return srcMgr().getPresumedLoc(loc);
    }

private:
    std::unique_ptr<clang::FrontendAction> action;
    std::unique_ptr<clang::CompilerInstance> m_Instance;
    std::unique_ptr<clang::syntax::TokenBuffer> m_TokBuf;
    std::unique_ptr<TemplateResolver> m_Resolver;
    llvm::DenseMap<clang::FileID, Directive> m_Directives;
    std::vector<std::string> m_Deps;
};

/// Build AST from given file path and content. If pch or pcm provided, apply them to the compiler.
/// Note this function will not check whether we need to update the PCH or PCM, caller should check
/// their reusability and update in time.
llvm::Expected<ASTInfo> compile(CompilationParams& params);

/// Run code completion at the given location.
llvm::Expected<ASTInfo> compile(CompilationParams& params, clang::CodeCompleteConsumer* consumer);

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
    std::optional<clang::PreambleBounds> bounds;

    /// Computes the preamble bounds for the given content.
    /// If the bounds are not provided explicitly, they will be calculated based on the content.
    ///
    /// - If the header is empty, the bounds can be determined by lexing the source file.
    /// - If the header is not empty, the preprocessor must be executed to compute the bounds.
    void computeBounds(llvm::StringRef header = "");

    llvm::IntrusiveRefCntPtr<vfs::FileSystem> vfs = new ThreadSafeFS();

    /// Remapped files. Currently, this is only used for testing.
    llvm::SmallVector<std::pair<std::string, std::string>> remappedFiles;

    /// Information about reuse PCH.
    std::string pch;
    clang::PreambleBounds pchBounds = {0, false};

    /// Information about reuse PCM(name, path).
    llvm::StringMap<std::string> pcms;

    /// Code completion file:line:column.
    llvm::StringRef file = "";
    uint32_t line = 0;
    uint32_t column = 0;

    void addPCH(const PCHInfo& info) {
        pch = info.path;
        /// pchBounds = info.bounds();
    }

    void addPCM(const PCMInfo& info) {
        assert((!pcms.contains(info.name) || pcms[info.name] == info.path) &&
               "Add a different PCM with the same name");
        pcms[info.name] = info.path;
    }
};

}  // namespace clice
