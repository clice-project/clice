#pragma once

#include "Preamble.h"

namespace clice {

struct ParsedAST {
    std::unique_ptr<clang::FrontendAction> action;
    std::unique_ptr<clang::CompilerInstance> instance;

    static std::unique_ptr<ParsedAST>
        build(llvm::StringRef filename, llvm::StringRef content, std::vector<const char*>& args, Preamble* preamble);

    auto& ASTContext() const { return instance->getASTContext(); }

    auto& SourceManager() const { return instance->getSourceManager(); }

    auto TranslationUnitDecl() const { return ASTContext().getTranslationUnitDecl(); }
};

}  // namespace clice
