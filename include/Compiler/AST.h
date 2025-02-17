#pragma once

#include "Directive.h"
#include "AST/Resolver.h"
#include "Basic/SourceCode.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/Syntax/Tokens.h"

namespace clice {

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
        m_directives(std::move(directives)), SM(this->instance->getSourceManager()) {}

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

    /// The interested file ID. For file without header context, it is the main file ID.
    /// For file with header context, it is the file ID of header file.
    clang::FileID getInterestedFile() {
        return interested;
    }

public:
    /// All files involved in building the AST.
    const llvm::DenseSet<clang::FileID>& files();

    std::vector<std::string> deps();

    clang::SourceLocation getSpellingLoc(clang::SourceLocation loc) {
        return SM.getSpellingLoc(loc);
    }

    clang::SourceLocation getExpansionLoc(clang::SourceLocation loc) {
        return SM.getExpansionLoc(loc);
    }

    auto getDecomposedLoc(clang::SourceLocation loc) {
        return SM.getDecomposedLoc(loc);
    }

    /// Get the file ID of a source location. The source location should always
    /// be a spelling location.
    clang::FileID getFileID(clang::SourceLocation spelling) {
        assert(spelling.isInvalid() && spelling.isFileID() && "Invalid source location");
        return SM.getFileID(spelling);
    }

    /// Get the file path of a file ID. If the file exists the path
    /// will be real path, otherwise it will be virtual path. The result
    /// makes sure the path is ended with '/0'.
    llvm::StringRef getFilePath(clang::FileID fid);

    /// Check if a file is a builtin file.
    bool isBuiltinFile(clang::FileID fid) {
        auto path = getFilePath(fid);
        return path == "<built-in>" || path == "<command line>" || path == "<scratch space>";
    }

    LocalSourceRange toLocalRange(clang::SourceRange range) {
        auto [begin, end] = range;
        assert(begin.isValid() && end.isValid() && "Invalid source range");
        assert(begin.isFileID() && end.isFileID() && "Invalid source range");
        auto [beginFID, beginOffset] = getDecomposedLoc(begin);
        auto [endFID, endOffset] = getDecomposedLoc(end);
        assert(beginFID == endFID && "Invalid source range");
        return LocalSourceRange{
            .begin = beginOffset,
            .end = endOffset + getTokenLength(SM, end),
        };
    }

private:
    /// The interested file ID.
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

    llvm::DenseSet<clang::FileID> allFiles;

    clang::SourceManager& SM;

    /// Cache for file path. It is used to avoid multiple file path lookup.
    llvm::DenseMap<clang::FileID, llvm::StringRef> pathCache;
    llvm::BumpPtrAllocator pathStorage;
};

}  // namespace clice
