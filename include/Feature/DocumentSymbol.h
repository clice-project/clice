#pragma once

#include "Server/Protocol.h"
#include "AST/SourceCode.h"
#include "AST/SymbolKind.h"
#include "Index/Shared.h"

namespace clice::feature {

struct DocumentSymbol {
    SymbolKind kind;

    std::string name;

    std::string detail;

    LocalSourceRange selectionRange;

    LocalSourceRange range;

    std::vector<DocumentSymbol> children;
};

std::vector<DocumentSymbol> documentSymbol(ASTInfo& AST);

index::Shared<std::vector<DocumentSymbol>> indexDocumentSymbol(ASTInfo& AST);

}  // namespace clice::feature

