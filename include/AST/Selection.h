#include "ParsedAST.h"

namespace clice {

class Selection {};

clang::Decl* test(clang::syntax::Token* token, ParsedAST& AST, llvm::StringRef filename);

}  // namespace clice
