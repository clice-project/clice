#pragma once

#include "AST/SymbolKind.h"
#include "AST/SourceCode.h"
#include "Index/Shared.h"

namespace clice::config {

struct SemanticTokensOption {};

};  // namespace clice::config

namespace clice::feature {

struct SemanticToken {
    LocalSourceRange range;
    SymbolKind kind;
    SymbolModifiers modifiers;
};

using SemanticTokens = std::vector<SemanticToken>;

/// Generate semantic tokens for the interested file only.
SemanticTokens semantic_tokens(CompilationUnit& unit);

/// Generate semantic tokens for all files.
index::Shared<SemanticTokens> index_semantic_token(CompilationUnit& unit);

}  // namespace clice::feature
