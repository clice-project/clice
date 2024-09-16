#include "Diagnostic.h"

namespace clice {

// TODO:

struct Directive {
    clang::SourceManager& sourceManager;
    llvm::StringSet<> includes;
    llvm::StringMap<std::vector<clang::SourceRange>> comments;
    
    clang::CommentHandler* handler();
    std::unique_ptr<clang::PPCallbacks> callback();
};

}  // namespace clice
