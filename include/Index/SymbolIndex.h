#pragma once

#include "Shared.h"
#include "AST/SymbolKind.h"
#include "AST/RelationKind.h"
#include "AST/SourceCode.h"

namespace clice::index {

Shared<std::vector<char>> index(ASTInfo& info);

}  // namespace clice::index
