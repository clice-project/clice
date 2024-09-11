#pragma once

#include <Protocol/SemanticTokens.h>

namespace clice {
struct ParsedAST;
}

namespace clice::feature {
protocol::SemanticTokens semanticTokens(const ParsedAST& AST, llvm::StringRef filename);
}  // namespace clice::feature

