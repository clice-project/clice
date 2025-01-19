#pragma once

#include "Shared.h"
#include "ArrayView.h"
#include "Basic/RelationKind.h"
#include "Basic/SymbolKind.h"
#include "Compiler/Compilation.h"
#include "Support/JSON.h"

namespace clice::index {

struct LocalSourceRange {
    /// The begin position offset to the source file.
    uint32_t begin;

    /// The end position offset to the source file.
    uint32_t end;
};

class SymbolIndex {
public:
    SymbolIndex(void* base, std::size_t size) : base(base), size(size) {}

    SymbolIndex(SymbolIndex&& other) : base(other.base), size(other.size) {
        other.base = nullptr;
        other.size = 0;
    }

    ~SymbolIndex() {
        std::free(base);
    }

    struct Symbol;

    struct Relation : Relative {
        /// Relation kind.
        RelationKind kind() const;

        /// Occurrence range.
        std::optional<LocalSourceRange> range() const;

        /// Whole symbol range.
        std::optional<LocalSourceRange> symbolRange() const;

        std::optional<Symbol> symbol() const;
    };

    struct SymbolID : Relative {
        /// Symbol id.
        uint64_t id() const;

        /// Symbol name.
        llvm::StringRef name() const;
    };

    struct Symbol : SymbolID {
        /// Symbol kind.
        SymbolKind kind() const;

        /// All relations of this symbol.
        ArrayView<Relation> relations() const;
    };

    struct Occurrence : Relative {
        /// Occurrence range.
        LocalSourceRange range() const;

        /// Occurrence symbol.
        Symbol symbol() const;
    };

    /// All symbols in the index.
    ArrayView<Symbol> symbols() const;

    /// All occurrences in the index.
    ArrayView<Occurrence> occurrences() const;

    /// Locate symbols at the given position.
    void locateSymbols(uint32_t position, llvm::SmallVectorImpl<Symbol>& symbols) const;

    /// Locate symbol with the given id(usually from another index).
    Symbol locateSymbol(SymbolID ID) const;

    json::Value toJSON() const;

public:
    void* base;
    std::size_t size;
};

Shared<SymbolIndex> index(ASTInfo& info);

}  // namespace clice::index
