#pragma once

#include "Preamble.h"

namespace clice {

struct ParsedAST {
    clang::ASTContext& context;
    clang::Preprocessor& preproc;
    clang::FileManager& fileManager;
    clang::SourceManager& sourceManager;
    clang::syntax::TokenBuffer tokenBuffer;
    std::unique_ptr<clang::FrontendAction> action;
    std::unique_ptr<clang::CompilerInstance> instance;

    clang::TranslationUnitDecl* tuDecl;

    static std::unique_ptr<ParsedAST>
        build(llvm::StringRef filename, llvm::StringRef content, std::vector<const char*>& args, Preamble* preamble);
};

}  // namespace clice
