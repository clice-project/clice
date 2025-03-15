#pragma once

#include "AST/SymbolKind.h"
#include "AST/SourceCode.h"
#include "Index/Shared.h"

namespace clice {

class ASTInfo;

namespace config {

struct SemanticTokensOption {};

};  // namespace config

namespace feature {

struct SemanticToken {
    LocalSourceRange range;
    SymbolKind kind;
    SymbolModifiers modifiers;
};

/// Generate semantic tokens for the interested file only.
std::vector<SemanticToken> semanticTokens(ASTInfo& AST);

/// Generate semantic tokens for all files.
index::Shared<std::vector<SemanticToken>> indexSemanticTokens(ASTInfo& AST);

}  // namespace feature

}  // namespace clice

