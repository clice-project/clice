#pragma once

#include <Support/ADT.h>

namespace clice {

/// A unique identifier for a symbol.
struct SymbolID {
    std::uint64_t value;
    llvm::StringRef USR;

    static SymbolID fromKind(std::uint64_t value) {
        return SymbolID{value, ""};
    }

    /// Create a SymbolID from a USR. Note that SymbolID doesn't own the USR string.
    static SymbolID fromUSR(llvm::StringRef USR) {
        return SymbolID{llvm::hash_value(USR), USR};
    }

    friend bool operator== (const SymbolID&, const SymbolID&) = default;

    friend std::strong_ordering operator<=> (const SymbolID& lhs, const SymbolID& rhs) {
        auto cmp = lhs.value <=> rhs.value;
        if(cmp != std::strong_ordering::equal) {
            return cmp;
        }
        return lhs.USR.compare(rhs.USR) <=> 0;
    }

    bool isUSR() const {
        return !USR.empty();
    }
};

}  // namespace clice

namespace llvm {

using clice::SymbolID;

template <>
struct DenseMapInfo<SymbolID> {
    inline static SymbolID getEmptyKey() {
        static SymbolID EMPTY_KEY = SymbolID::fromKind(std::numeric_limits<uint64_t>::max());
        return EMPTY_KEY;
    }

    inline static SymbolID getTombstoneKey() {
        static SymbolID TOMBSTONE_KEY = SymbolID::fromKind(std::numeric_limits<uint64_t>::max() - 1);
        return TOMBSTONE_KEY;
    }

    inline static llvm::hash_code getHashValue(const SymbolID& ID) {
        return ID.value;
    }

    inline static bool isEqual(const SymbolID& LHS, const SymbolID& RHS) {
        return LHS == RHS;
    }
};

}  // namespace llvm
