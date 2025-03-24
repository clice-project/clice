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

    /// Return the hash value of symbol id.
    std::uint64_t hash() const;

    /// Return the symbol name.
    llvm::StringRef name() const;

    /// Return the symbol kind.
    SymbolKind kind() const;

    /// Return all relations of this symbol.
    LazyArray<Relation> relations() const;
};

struct Occurrence : Relative {
    /// The source range of occurrence.
    LocalSourceRange range() const;

    /// The target symbol of occurrence.
    Symbol symbol() const;
};

class SymbolIndex {
public:
    SymbolIndex(const char* data, std::uint32_t size) : data(data), size(size) {}

    /// The path of source file.
    llvm::StringRef path() const;

    /// The content of source file.
    llvm::StringRef content() const;

    /// All symbols in the index.
    LazyArray<Symbol> symbols() const;

    /// All occurrences in the index.
    LazyArray<Occurrence> occurrences() const;

    /// Locate the symbols with given offset.
    std::vector<Symbol> locateSymbol(uint32_t offset) const;

    /// Locate the symbol with given symbol id.
    std::optional<Symbol> locateSymbol(const SymbolID& id) const;

    static Shared<std::vector<char>> build(ASTInfo& AST);

private:
    const char* data;
    std::uint32_t size;
};

}  // namespace clice::index
