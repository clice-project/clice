#include "ParsedAST.h"
#include <clang/AST/ASTTypeTraits.h>

namespace clice {

// Code Action:
// add implementation in cpp file(important).
// extract implementation to cpp file(important).
// generate virtual function declaration(full qualified?).
// generate c++20 coroutine and awaiter interface.
// expand macro(one step by step).
// invert if.

class SelectionTree {
public:
    SelectionTree(clang::ASTContext& context,
                  const clang::syntax::TokenBuffer& tokens,
                  clang::SourceLocation begin,
                  clang::SourceLocation end);
};

}  // namespace clice
