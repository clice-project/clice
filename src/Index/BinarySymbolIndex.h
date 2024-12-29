#include "Index/SymbolIndex.h"

namespace clice::index {

/// This namespace defines the binary format of the index file. Generally,
/// transform all pointer to offset to base address and cache location in the
/// location array. And data only will be deserialized when it is accessed.
namespace binary {

template <typename T>
struct Array {
    /// offset to index start.
    uint32_t offset;

    /// number of elements.
    uint32_t size;
};

using String = Array<char>;

struct Relation {
    RelationKind kind;
    uint32_t location = std::numeric_limits<uint32_t>::max();
    uint32_t extra = std::numeric_limits<uint32_t>::max();
};

struct Symbol {
    uint64_t id;
    String name;
    SymbolKind kind;
    Array<Relation> relations;
};

struct Occurrence {
    uint32_t location = std::numeric_limits<uint32_t>::max();
    uint32_t symbol = std::numeric_limits<uint32_t>::max();
};

struct SymbolIndex {
    Array<Symbol> symbols;
    Array<Occurrence> occurrences;
    Array<Location> locations;
};

struct ProxyIndex : SymbolIndex {
    llvm::StringRef getString(String string) const {
        return {reinterpret_cast<const char*>(this) + string.offset, string.size};
    }

    template <typename T>
    llvm::ArrayRef<T> getArray(Array<T> array) const {
        return {reinterpret_cast<const T*>(reinterpret_cast<const char*>(this) + array.offset),
                array.size};
    }

    llvm::ArrayRef<Symbol> getSymbols() const {
        return getArray(symbols);
    }

    llvm::ArrayRef<Occurrence> getOccurrences() const {
        return getArray(occurrences);
    }

    llvm::ArrayRef<Location> getLocations() const {
        return getArray(locations);
    }

    template <typename To, typename From>
    ArrayView<To> getArrayView(Array<From> array) const {
        auto base = reinterpret_cast<const char*>(this);
        return {base, base + array.offset, array.size, sizeof(From)};
    }
};

}  // namespace binary

}  // namespace clice::index
