#pragma once

#include "AST/Resolver.h"
#include "AST/SourceCode.h"
#include "AST/SymbolID.h"
#include "Directive.h"

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
          llvm::DenseMap<clang::FileID, Directive> directives)
      : interested(interested), action(std::move(action)),
        instance(std::move(instance)), m_resolver(std::move(resolver)),
        buffer(std::move(buffer)), m_directives(std::move(directives)),
        SM(this->instance->getSourceManager()) {}

  ASTInfo(const ASTInfo &) = delete;

  ASTInfo(ASTInfo &&) = default;

  ~ASTInfo() {
    if (action) {
      action->EndSourceFile();
    }
  }

public:
  auto &srcMgr() { return instance->getSourceManager(); }

  auto &pp() { return instance->getPreprocessor(); }

  auto &context() { return instance->getASTContext(); }

  auto &sema() { return instance->getSema(); }

  auto &tokBuf() {
    assert(buffer && "Token buffer is not available");
    return *buffer;
  }

  auto &resolver() {
    assert(m_resolver && "Template resolver is not available");
    return *m_resolver;
  }

  auto &directives() { return m_directives; }

  auto tu() { return instance->getASTContext().getTranslationUnitDecl(); }

  /// The interested file ID. For file without header context, it is the main
  /// file ID. For file with header context, it is the file ID of header file.
  clang::FileID getInterestedFile() const { return interested; }

  llvm::StringRef getInterestedFileContent() const {
    return getFileContent(interested);
  }

  /// All files involved in building the AST.
  const llvm::DenseSet<clang::FileID> &files();

  std::vector<std::string> deps();

  clang::SourceLocation getSpellingLoc(clang::SourceLocation loc) const {
    return SM.getSpellingLoc(loc);
  }

  clang::SourceLocation getExpansionLoc(clang::SourceLocation loc) const {
    return SM.getExpansionLoc(loc);
  }

  auto getDecomposedLoc(clang::SourceLocation loc) const {
    return SM.getDecomposedLoc(loc);
  }

  /// Get the file ID of a source location. The source location should always
  /// be a spelling location.
  clang::FileID getFileID(clang::SourceLocation spelling) {
    assert(spelling.isValid() && spelling.isFileID() &&
           "Invalid source location");
    return SM.getFileID(spelling);
  }

  /// Get the file path of a file ID. If the file exists the path
  /// will be real path, otherwise it will be virtual path. The result
  /// makes sure the path is ended with '/0'.
  llvm::StringRef getFilePath(clang::FileID fid);

  /// Get the content of a file ID.
  llvm::StringRef getFileContent(clang::FileID fid) const {
    return SM.getBufferData(fid);
  }

  /// Check if a file is a builtin file.
  bool isBuiltinFile(clang::FileID fid) {
    auto path = getFilePath(fid);
    return path == "<built-in>" || path == "<command line>" ||
           path == "<scratch space>";
  }

  /// Decompose a source range into file ID and local source range. The begin
  /// and end of the input source range both should be `FileID`. If the range is
  /// cross multiple files, we cut off the range at the end of the first file.
  std::pair<clang::FileID, LocalSourceRange>
  toLocalRange(clang::SourceRange range);

  /// Same as `toLocalRange`, but will translate range to expansion range.
  std::pair<clang::FileID, LocalSourceRange>
  toLocalExpansionRange(clang::SourceRange range);

  Location toLocation(clang::SourceRange range) {
    auto [fid, localRange] = toLocalRange(range);
    return Location{
        .file = getFilePath(fid).str(),
        .range = localRange,
    };
  }

  /// Get symbol ID for given declaration.
  index::SymbolID getSymbolID(const clang::NamedDecl *decl);

  /// Get symbol ID for given marco.
  index::SymbolID getSymbolID(const clang::MacroInfo *macro);

private:
  /// The interested file ID.
  clang::FileID interested;

  /// The frontend action used to build the AST.
  std::unique_ptr<clang::FrontendAction> action;

  /// Compiler instance, responsible for performing the actual compilation and
  /// managing the lifecycle of all objects during the compilation process.
  std::unique_ptr<clang::CompilerInstance> instance;

  /// The template resolver used to resolve dependent name.
  std::optional<TemplateResolver> m_resolver;

  /// Token information collected during the preprocessing.
  std::optional<clang::syntax::TokenBuffer> buffer;

  /// All diretive information collected during the preprocessing.
  llvm::DenseMap<clang::FileID, Directive> m_directives;

  llvm::DenseSet<clang::FileID> allFiles;

  clang::SourceManager &SM;

  /// Cache for file path. It is used to avoid multiple file path lookup.
  llvm::DenseMap<clang::FileID, llvm::StringRef> pathCache;

  /// Cache for symbol id.
  llvm::DenseMap<const void *, uint32_t> symbolHashCache;

  llvm::BumpPtrAllocator pathStorage;
};

} // namespace clice
