#pragma once

#include "Preamble.h"
#include "Module.h"
#include "Directive.h"
#include "Resolver.h"

#include "clang/Frontend/CompilerInstance.h"

namespace clice {

struct CompilationParams;

/// All AST related information needed for language server.
class ASTInfo {
public:
    ASTInfo(clang::FileID interested,
            std::unique_ptr<clang::FrontendAction> action,
            std::unique_ptr<clang::CompilerInstance> instance,
            std::optional<TemplateResolver> resolver,
            std::optional<clang::syntax::TokenBuffer> buffer,
            llvm::DenseMap<clang::FileID, Directive> directives) :
        interested(interested), action(std::move(action)), instance(std::move(instance)),
        m_resolver(std::move(resolver)), buffer(std::move(buffer)),
        m_directives(std::move(directives)) {}

    ASTInfo(const ASTInfo&) = delete;

    ASTInfo(ASTInfo&&) = default;

    ~ASTInfo() {
        if(action) {
            action->EndSourceFile();
        }
    }

public:
    auto& srcMgr() {
        return instance->getSourceManager();
    }

    auto& pp() {
        return instance->getPreprocessor();
    }

    auto& context() {
        return instance->getASTContext();
    }

    auto& sema() {
        return instance->getSema();
    }

    auto& tokBuf() {
        assert(buffer && "Token buffer is not available");
        return *buffer;
    }

    auto& resolver() {
        assert(m_resolver && "Template resolver is not available");
        return *m_resolver;
    }

    auto& directives() {
        return m_directives;
    }

    auto tu() {
        return instance->getASTContext().getTranslationUnitDecl();
    }

    /// ============================================================================
    ///                            Utility Functions
    /// ============================================================================

    /// @brief Get the length of the token at the given location.
    /// All SourceLocation instances in the Clang AST originate from the start position of tokens,
    /// which helps reduce memory usage. When token length information is needed, a simple lexing
    /// operation based on the start position can be performed.
    auto getTokenLength(clang::SourceLocation loc) {
        return clang::Lexer::MeasureTokenLength(loc, srcMgr(), instance->getLangOpts());
    }

    /// @brief Get the spelling of the token at the given location.
    llvm::StringRef getTokenSpelling(clang::SourceLocation loc) {
        return llvm::StringRef(srcMgr().getCharacterData(loc), getTokenLength(loc));
    }

private:
    /// The interested file ID. For file without header context, it is the main file ID.
    /// For file with header context, it is the file ID of header file.
    clang::FileID interested;

    /// The frontend action used to build the AST.
    std::unique_ptr<clang::FrontendAction> action;

    /// Compiler instance, responsible for performing the actual compilation and managing the
    /// lifecycle of all objects during the compilation process.
    std::unique_ptr<clang::CompilerInstance> instance;

    /// The template resolver used to resolve dependent name.
    std::optional<TemplateResolver> m_resolver;

    /// Token information collected during the preprocessing.
    std::optional<clang::syntax::TokenBuffer> buffer;

    /// All diretive information collected during the preprocessing.
    llvm::DenseMap<clang::FileID, Directive> m_directives;
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
    std::optional<std::uint32_t> bounds;

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
