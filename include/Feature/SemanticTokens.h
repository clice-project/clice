#pragma once

#include <Protocol/SemanticTokens.h>

namespace clice {
struct ParsedAST;
}

namespace clice::feature {
proto::SemanticTokens semanticTokens(const ParsedAST& AST, llvm::StringRef filename);
}  // namespace clice::feature

