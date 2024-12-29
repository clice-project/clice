#pragma once

#include "Basic/RelationKind.h"

namespace clice::index {

struct Position {
    uint32_t line;
    uint32_t column;
};

struct Location {
    Position begin;
    Position end;
};

/// When serialize index to binary, we will transform all pointer to offset
/// to base address. And data only will be deserialized when it is accessed.
struct Relative {
    const void* base;
    const void* data;
};

template <typename T>
class ArrayView : Relative {
public:
    ArrayView(const void* base, const void* data, std::size_t size, std::size_t stride) :
        Relative{base, data}, size(size), stride(stride) {}

    class Iterator : Relative {
    public:
        Iterator(const void* base, const void* data, std::size_t stride) :
            Relative{base, data}, stride(stride) {}

        T operator* () {
            return T{base, data};
        }

        Iterator& operator++ () {
            data = static_cast<const char*>(data) + stride;
            return *this;
        }

        friend bool operator== (const Iterator& lhs, const Iterator& rhs) = default;

    private:
        std::size_t stride;
    };

    Iterator begin() {
        return Iterator(base, data, stride);
    }

    Iterator end() {
        return Iterator(base, static_cast<const char*>(data) + size * stride, stride);
    }

    friend bool operator== (const ArrayView& lhs, const ArrayView& rhs) = default;

private:
    std::size_t size;
    std::size_t stride;
};

class SymbolIndex {
public:
    struct Symbol;

    struct Relation : Relative {
        /// Relation kind.
        RelationKind kind();

        /// Occurrence range.
        Location range();

        Symbol symbol();
    };

    struct SymbolID : Relative {
        /// Symbol id.
        int64_t id();

        /// Symbol name.
        llvm::StringRef name();
    };

    struct Symbol : SymbolID {
        /// all relations of this symbol.
        ArrayView<Relation> relations();
    };

    struct Occurrence : Relative {
        /// Occurrence range.
        Location location();

        /// Occurrence symbol.
        Symbol symbol();
    };

    /// All symbols in the index.
    ArrayView<Symbol> symbols();

    /// All occurrences in the index.
    ArrayView<Occurrence> occurrences();

    /// Locate symbols at the given position.
    ArrayView<Symbol> locateSymbols(Position position);

    /// Locate symbol with the given id(usually from another index).
    Symbol locateSymbol(SymbolID ID);

private:
    void* base;
    std::size_t size;
};

}  // namespace clice::index
