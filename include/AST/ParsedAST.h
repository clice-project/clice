#pragma once

#include "Preamble.h"

namespace clice {

struct ParsedAST {
    clang::Sema& sema;
    clang::ASTContext& context;
    clang::Preprocessor& preproc;
    clang::FileManager& fileManager;
    clang::SourceManager& sourceManager;
    clang::syntax::TokenBuffer tokenBuffer;
    std::unique_ptr<Directive> directive;
    std::unique_ptr<clang::FrontendAction> action;
    std::unique_ptr<clang::CompilerInstance> instance;

    static std::unique_ptr<ParsedAST> build(llvm::StringRef filename,
                                            llvm::StringRef content,
                                            std::vector<const char*>& args,
                                            Preamble* preamble = nullptr);

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
