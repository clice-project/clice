#pragma once

#include "Shared.h"
#include "AST/SymbolID.h"
#include "AST/SourceCode.h"
#include "AST/SymbolKind.h"
#include "AST/RelationKind.h"

namespace clice::index {

class SymbolIndex {
public:
    SymbolIndex(const char* data, std::uint32_t size) : data(data), size(size) {}

    /// The path of source file.
    llvm::StringRef path();

    /// The content of source file.
    llvm::StringRef content();

    /// Locate the symbols with given offset.
    std::vector<SymbolID> locateSymbol(uint32_t offset);

    void locateSymbol(const SymbolID& id, RelationKind kind);

private:
    const char* data;
    std::uint32_t size;
};

Shared<std::vector<char>> index(ASTInfo& info);

}  // namespace clice::index
