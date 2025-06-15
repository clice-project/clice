#pragma once

#include "Directive.h"
#include "AST/SymbolID.h"
#include "AST/SourceCode.h"
#include "AST/Resolver.h"
#include "clang/Tooling/Syntax/Tokens.h"

namespace clice {

/// All AST related information needed for language server.
class CompilationUnit {
public:
    /// The kind describes how we preprocess ths source file
    /// to get this compilation unit.
    enum class Kind : std::uint8_t {
        /// From preprocessing the source file. Therefore diretives
        /// are available but AST nodes are not.
        Preprocess,

        /// From indexing the static source file.
        Indexing,

        /// From building preamble for the source file.
        Preamble,

        /// From building precompiled module for the module interface unit.
        ModuleInterface,

        /// From building normal AST for source file, interested file and top level
        /// declarations are available.
        SyntaxOnly,

        /// From running code completion for the source file(preamble is applied).
        Completion,
    };

    using enum Kind;
    struct Impl;

    CompilationUnit(Kind kind, Impl* impl) : kind(kind), impl(impl) {}

    CompilationUnit(const CompilationUnit&) = delete;

    CompilationUnit(CompilationUnit&& other) : kind(other.kind), impl(other.impl) {
        other.impl = nullptr;
    }

    ~CompilationUnit();

public:
    clang::FileID file_id(llvm::StringRef file);

    clang::FileID file_id(clang::SourceLocation location);

    std::uint32_t file_offset(clang::SourceLocation location);

    /// Get the file path of a file ID. If the file exists the path
    /// will be real path, otherwise it will be virtual path. The result
    /// makes sure the path is ended with '/0'.
    llvm::StringRef file_path(clang::FileID fid);

    /// Get the content of a file ID.
    llvm::StringRef file_content(clang::FileID fid);

    clang::SourceLocation start_location(clang::FileID fid);

    clang::SourceLocation end_location(clang::FileID fid);

    clang::SourceLocation include_location(clang::FileID fid);

    /// Given a macro location, return its top level spelling location(the location
    // of the token that the result token is expanded from, may from macro argument
    // or macro definition).
    clang::SourceLocation spelling_location(clang::SourceLocation location);

    /// Given a macro location, return its top level expansion location(the location of
    // macro expansion).
    clang::SourceLocation expansion_location(clang::SourceLocation location);

    std::pair<clang::FileID, std::uint32_t> decompose_location(clang::SourceLocation location);

    /// Decompose a source range into file ID and local source range. The begin and end
    /// of the input source range both should be `FileID`. If the range is cross multiple
    /// files, we cut off the range at the end of the first file.
    std::pair<clang::FileID, LocalSourceRange> decompose_range(clang::SourceRange range);

    /// Same as `toLocalRange`, but will translate range to expansion range.
    std::pair<clang::FileID, LocalSourceRange> decompose_expansion_range(clang::SourceRange range);

    clang::PresumedLoc presumed_location(clang::SourceLocation location);

    llvm::ArrayRef<clang::syntax::Token> spelled_tokens(clang::FileID fid);

    llvm::ArrayRef<clang::syntax::Token> expanded_tokens(clang::SourceRange range);

    llvm::StringRef token_spelling(clang::SourceLocation location);

    llvm::StringRef module_name();

    bool is_module_interface_unit();

    clang::LangOptions& lang_options();

public:
    clang::ASTContext& context();

    clang::Sema& sema();

    TemplateResolver& resolver();

    llvm::DenseMap<clang::FileID, Directive>& directives();

    clang::TranslationUnitDecl* tu();

    /// The interested file ID. For file without header context, it is the main file ID.
    /// For file with header context, it is the file ID of header file.
    clang::FileID getInterestedFile();

    llvm::StringRef getInterestedFileContent();

    /// All files involved in building the unit.
    const llvm::DenseSet<clang::FileID>& files();

    std::vector<std::string> deps();

    /// Check if a file is a builtin file.
    bool isBuiltinFile(clang::FileID fid);

    /// Get symbol ID for given declaration.
    index::SymbolID getSymbolID(const clang::NamedDecl* decl);

    /// Get symbol ID for given marco.
    index::SymbolID getSymbolID(const clang::MacroInfo* macro);

private:
    Kind kind;

    Impl* impl;
};

}  // namespace clice
