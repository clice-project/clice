#pragma once

#include <Compiler/Clang.h>

namespace clice {

struct ParsedAST {
    clang::Sema& sema;
    clang::ASTContext& context;
    clang::Preprocessor& preproc;
    clang::FileManager& fileManager;
    clang::SourceManager& sourceManager;
    clang::syntax::TokenBuffer tokenBuffer;
    // std::unique_ptr<Directives> directive;
    std::unique_ptr<clang::FrontendAction> action;
    std::unique_ptr<clang::CompilerInstance> instance;

    clang::FileID getFileID(llvm::StringRef filename) const {
        auto entry = fileManager.getFileRef(filename);
        if(!entry) {
            // TODO:
        }
        return sourceManager.translateFile(entry.get());
    }

    llvm::ArrayRef<clang::syntax::Token> spelledTokens(clang::FileID fileID) const {
        return tokenBuffer.spelledTokens(fileID);
    }
};

}  // namespace clice
