#pragma once

#include <Protocol/SemanticTokens.h>
#include <llvm/ADT/StringRef.h>

namespace clice {
struct ParsedAST;
}

namespace clice::feature {
proto::SemanticTokens semanticTokens(const ParsedAST& AST, llvm::StringRef filename);
}  // namespace clice::feature

