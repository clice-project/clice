#pragma once

#include <Protocol/Language/SemanticToken.h>

namespace clice {

class ParsedAST;

namespace feature {

protocol::SemanticTokens semanticTokens(const ParsedAST& ast);



}

}  // namespace clice

