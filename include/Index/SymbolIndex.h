#pragma once

#include "Shared.h"
#include "LazyArray.h"
#include "AST/SymbolID.h"
#include "AST/SourceCode.h"
#include "AST/SymbolKind.h"
#include "AST/RelationKind.h"

namespace clice::index {

class Symbol;

struct Relation : Relative {
    /// Return the relation kind.
    RelationKind kind() const;

    /// Return the range of relation.
    LocalSourceRange range() const;

    /// Return the definition range.
    LocalSourceRange definitionRange() const;

    /// The the target symbol.
    Symbol target() const;
};

struct Symbol : Relative {
    /// Return the symbol id.
    SymbolID id() const;

    /// Return the symbol kind.
    SymbolKind kind() const;

    /// Return all relations of this symbol.
    LazyArray<Relation> relations() const;
};

class SymbolIndex {
public:
    SymbolIndex(const char* data, std::uint32_t size) : data(data), size(size) {}

    /// The path of source file.
    llvm::StringRef path();

    /// The content of source file.
    llvm::StringRef content();

    /// Locate the symbols with given offset.
    std::vector<Symbol> locateSymbol(uint32_t offset);

    /// Locate the symbol with given symbol id.
    Symbol locateSymbol(const SymbolID& id);

private:
    const char* data;
    std::uint32_t size;
};

Shared<std::vector<char>> index(ASTInfo& info);

}  // namespace clice::index
