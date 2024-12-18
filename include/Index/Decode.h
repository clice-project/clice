#include "Binary.h"

namespace clice::index::decode {

/// This file defines proxy objects for lazily decoding the binary index data
/// into more convenient representations. These objects rely on pointers, so
/// ensure the binary index is still alive while using them.

struct Proxy {
    const binary::Index& index;  ///< The binary index containing the data.

    /// Decodes a string from the binary index.
    llvm::StringRef getString(binary::String string) const {
        return llvm::StringRef((const char*)&index + string.offset, string.size);
    }

    /// Decodes an array from the binary index.
    template <typename T>
    llvm::ArrayRef<T> getArray(binary::Array<T> array) const {
        return llvm::ArrayRef<T>(reinterpret_cast<const T*>((const char*)&index + array.offset),
                                 array.size);
    }
};

/// A proxy for lazily decoding an array of objects from the binary index.
template <typename From, typename To>
struct Array {
    const binary::Index& index;        ///< The binary index containing the data.
    const llvm::ArrayRef<From> array;  ///< The array of raw data.

    /// Iterator for the array, allowing iteration over decoded objects.
    struct iterator {
        const binary::Index* index;  ///< The binary index.
        const From* ptr;             ///< Pointer to the current element.

        To operator* () const {
            return To{*index, *ptr};
        }

        iterator& operator++ () {
            ++ptr;
            return *this;
        }

        bool operator== (const iterator& other) const = default;
    };

    auto begin() const {
        return iterator{&index, array.begin()};
    }

    auto end() const {
        return iterator{&index, array.end()};
    }
};

struct File;

/// Represents a decoded location in the binary index.
struct Location : Proxy {
    const index::Location& location;  ///< The raw location data.

    File file() const;  ///< Retrieves the file for this location.

    proto::Range range() const {
        return location.range;
    }
};

/// Represents a decoded file in the binary index.
struct File : Proxy {
    const binary::File& file;  ///< The raw file data.

    llvm::StringRef path() const {
        return getString(file.path);
    }

    Location include() const {
        return {*this, getArray(index.locations)[file.include.offset]};
    }
};

inline File Location::file() const {
    return {index, getArray(index.files)[location.file.offset]};
}

/// Represents a decoded symbol in the binary index.
struct Symbol : Proxy {
    const binary::Symbol& symbol;  ///< The raw symbol data.

    uint64_t id() const {
        return symbol.id;
    }

    llvm::StringRef name() const {
        return getString(symbol.name);
    }

    Array<Relation, index::Relation> relations() const {
        return {index, getArray(symbol.relations)};
    }
};

/// Represents a decoded occurrence of a symbol in the binary index.
struct Occurrence : Proxy {
    const index::Occurrence& occurrence;  ///< The raw occurrence data.

    Location location() const {
        return {*this, getArray(index.locations)[occurrence.location.offset]};
    }

    Symbol symbol() const {
        return {*this, getArray(index.symbols)[occurrence.symbol.offset]};
    }
};

/// Represents the decoded binary index.
struct Index : Proxy {
    llvm::StringRef version() const {
        return getString(index.version);
    }

    llvm::StringRef language() const {
        return getString(index.language);
    }

    llvm::StringRef path() const {
        return getString(index.path);
    }

    llvm::StringRef context() const {
        return getString(index.content);
    }

    Array<binary::File, File> files() const {
        return {index, getArray(index.files)};
    }

    Array<binary::Symbol, Symbol> symbols() const {
        return {index, getArray(index.symbols)};
    }

    Array<index::Occurrence, Occurrence> occurrences() const {
        return {index, getArray(index.occurrences)};
    }
};

}  // namespace clice::index::decode
