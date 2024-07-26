#pragma once

#include "Diagnostic.h"
#include "Preamble.h"
#include "CompileDatabase.h"

namespace clice {

class ParsedAST {
private:
    using Decl = clang::Decl*;
    using TokenBuffer = clang::syntax::TokenBuffer;
    using ASTConsumer = std::unique_ptr<clang::ASTConsumer>;

    struct FrontendAction : public clang::ASTFrontendAction {
        ASTConsumer CreateASTConsumer(CompilerInstance& instance, PathRef file) override;
    };

private:
    /// path of translation unit
    PathRef path;
    /// llvm version
    std::string version;
    /// headers part of the tu, when a file is loaded, we will build the preamble, the reuse it.
    Preamble preamble;
    /// some extra info
    std::vector<Decl> topLevelDecls;
    std::vector<Diagnostic> diagnostics;
    /// core members for clang frontend
    // uninitialized<TokenBuffer> tokens;
    clang::SyntaxOnlyAction action;
    CompilerInstance instance;
    clang::syntax::TokenBuffer buffer;

public:
    static std::unique_ptr<ParsedAST> build(std::string_view path, std::string_view content);

    static std::unique_ptr<ParsedAST> build(std::string_view path,
                                            const std::shared_ptr<CompilerInvocation>& invocation,
                                            const Preamble& preamble);

    auto& Diagnostics() { return diagnostics; }

    auto& ASTContext() { return instance.getASTContext(); }

    auto& TokensBuffer() { return buffer; }

    auto& SourceManager() { return instance.getSourceManager(); }

    auto& TranslationUnit() { return *instance.getASTContext().getTranslationUnitDecl(); }
};

}  // namespace clice
