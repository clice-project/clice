#pragma once

#include <Support/ADT.h>

namespace clice {

class SymbolID {
public:
    SymbolID() = default;

    explicit SymbolID(llvm::StringRef USR) {
        this->USR = USR;
        m_hash = llvm::xxHash64(USR);
    }

    bool operator== (const SymbolID&) const = default;

    llvm::hash_code hash() const {
        return m_hash;
    }

private:
    llvm::StringRef USR;
    std::uint64_t m_hash;
};

}  // namespace clice

namespace llvm {

template <>
struct DenseMapInfo<clice::SymbolID> {
    inline static clice::SymbolID getEmptyKey() {
        static clice::SymbolID EMPTY_KEY("EMPTY_KEY");
        return EMPTY_KEY;
    }

    inline static clice::SymbolID getTombstoneKey() {
        static clice::SymbolID TOMBSTONE_KEY("TOMBSTONE_KEY");
        return TOMBSTONE_KEY;
    }

    inline static llvm::hash_code getHashValue(const clice::SymbolID& ID) {
        return ID.hash();
    }

    inline static bool isEqual(const clice::SymbolID& LHS, const clice::SymbolID& RHS) {
        return LHS == RHS;
    }
};

}  // namespace llvm
