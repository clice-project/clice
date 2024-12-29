#pragma once

#include "Basic/RelationKind.h"
#include "Basic/SymbolKind.h"
#include "Compiler/Compiler.h"
#include "Support/JSON.h"

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

    bool operator== (const Relative& other) const = default;
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

        T operator* () const {
            return T{base, data};
        }

        Iterator& operator++ () {
            data = static_cast<const char*>(data) + stride;
            return *this;
        }

        bool operator== (const Iterator& other) const = default;

    private:
        std::size_t stride;
    };

    Iterator begin() const {
        return Iterator(base, data, stride);
    }

    Iterator end() const {
        return Iterator(base, static_cast<const char*>(data) + size * stride, stride);
    }

    uint32_t length() const {
        return size;
    }

    T operator[] (uint32_t index) const {
        return T{base, static_cast<const char*>(data) + index * stride};
    }

    bool operator== (const ArrayView& other) const = default;

private:
    std::size_t size;
    std::size_t stride;
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
        Location range() const;

        Symbol symbol() const;
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
        Location location() const;

        /// Occurrence symbol.
        Symbol symbol() const;
    };

    /// All symbols in the index.
    ArrayView<Symbol> symbols() const;

    /// All occurrences in the index.
    ArrayView<Occurrence> occurrences() const;

    /// Locate symbols at the given position.
    ArrayView<Symbol> locateSymbols(Position position) const;

    /// Locate symbol with the given id(usually from another index).
    Symbol locateSymbol(SymbolID ID) const;

    json::Value toJSON() const;

public:
    void* base;
    std::size_t size;
};

llvm::DenseMap<clang::FileID, SymbolIndex> test(ASTInfo& info);

}  // namespace clice::index
