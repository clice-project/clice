#include "Diagnostic.h"

namespace clice {

// TODO:

struct Directive {
    llvm::StringSet<> includes;
    std::vector<clang::SourceRange> comments;

    clang::CommentHandler* handler();
    std::unique_ptr<clang::PPCallbacks> callback();
};

}  // namespace clice
