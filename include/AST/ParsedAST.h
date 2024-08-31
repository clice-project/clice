#pragma once

#include "Preamble.h"

namespace clice {

struct ParsedAST {
    clang::Preprocessor& pp;
    clang::SourceManager& sm;
    clang::ASTContext& context;
    clang::syntax::TokenBuffer tb;
    clang::TranslationUnitDecl* tu;
    std::unique_ptr<clang::FrontendAction> action;
    std::unique_ptr<clang::CompilerInstance> instance;

    static std::unique_ptr<ParsedAST>
        build(llvm::StringRef filename, llvm::StringRef content, std::vector<const char*>& args, Preamble* preamble);
};

}  // namespace clice
