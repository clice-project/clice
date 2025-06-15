#pragma once

#include "Directive.h"
#include "AST/SymbolID.h"
#include "AST/SourceCode.h"
#include "AST/Resolver.h"

#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/Syntax/Tokens.h"

namespace clice {

/// All AST related information needed for language server.
class CompilationUnit {
public:
    CompilationUnit(clang::FileID interested,
                    std::unique_ptr<clang::FrontendAction> action,
                    std::unique_ptr<clang::CompilerInstance> instance,
                    std::optional<TemplateResolver> resolver,
                    std::optional<clang::syntax::TokenBuffer> buffer,
                    llvm::DenseMap<clang::FileID, Directive> directives) :
        interested(interested), action(std::move(action)), instance(std::move(instance)),
        m_resolver(std::move(resolver)), buffer(std::move(buffer)),
        m_directives(std::move(directives)), SM(this->instance->getSourceManager()) {}

    CompilationUnit(const CompilationUnit&) = delete;

    CompilationUnit(CompilationUnit&&) = default;

    ~CompilationUnit() {
        if(action) {
            action->EndSourceFile();
        }
    }

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

public:
    clang::FileID file_id(llvm::StringRef file);

    clang::FileID file_id(clang::SourceLocation location);

    std::uint32_t file_offset(clang::SourceLocation location);

    /// Get the file path of a file ID. If the file exists the path
    /// will be real path, otherwise it will be virtual path. The result
    /// makes sure the path is ended with '/0'.
    llvm::StringRef file_path(clang::FileID fid);

    /// Get the content of a file ID.
    llvm::StringRef file_content(clang::FileID fid) const {
        return SM.getBufferData(fid);
    }

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
    auto& context() {
        return instance->getASTContext();
    }

    auto& sema() {
        return instance->getSema();
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
    clang::FileID getInterestedFile() const {
        return interested;
    }

    llvm::StringRef getInterestedFileContent() const {
        return file_content(interested);
    }

    /// All files involved in building the unit.
    const llvm::DenseSet<clang::FileID>& files();

    std::vector<std::string> deps();

    /// Check if a file is a builtin file.
    bool isBuiltinFile(clang::FileID fid) {
        auto path = file_path(fid);
        return path == "<built-in>" || path == "<command line>" || path == "<scratch space>";
    }

    /// Get symbol ID for given declaration.
    index::SymbolID getSymbolID(const clang::NamedDecl* decl);

    /// Get symbol ID for given marco.
    index::SymbolID getSymbolID(const clang::MacroInfo* macro);

private:
    /// The interested file ID.
    clang::FileID interested;

    /// The frontend action used to build the unit.
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

    /// Cache for symbol id.
    llvm::DenseMap<const void*, std::uint64_t> symbolHashCache;

    llvm::BumpPtrAllocator pathStorage;

    std::vector<clang::Decl*> top_level_decls;
};

}  // namespace clice
