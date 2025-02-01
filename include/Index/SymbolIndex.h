#pragma once

#include "Shared.h"
#include "ArrayView.h"
#include "Basic/SourceCode.h"
#include "Basic/SymbolKind.h"
#include "Basic/RelationKind.h"
#include "Support/JSON.h"

namespace clice {

class ASTInfo;

}

namespace clice::index {

class SymbolIndex {
public:
    SymbolIndex(char* base, std::size_t size, bool own = true) : base(base), size(size), own(own) {}

    SymbolIndex(SymbolIndex&& other) noexcept : base(other.base), size(other.size), own(other.own) {
        other.base = nullptr;
        other.size = 0;
        other.own = false;
    }

    ~SymbolIndex() {
        if(own) {
            std::free(base);
        }
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
    std::optional<Symbol> locateSymbol(uint64_t id, llvm::StringRef name) const;

    json::Value toJSON() const;

public:
    char* base;
    std::size_t size;
    bool own;
};

Shared<SymbolIndex> index(ASTInfo& info);

}  // namespace clice::index
