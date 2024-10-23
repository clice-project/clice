#pragma once

#include <Support/ADT.h>
#include <Protocol/Basic.h>

// #include <Index/SymbolID.h>

namespace clice::index {

/// Note that we have two kinds of `Index` definitions. One for collecting data from AST,
/// and the other is used to serialize data to the binary format. The key difference is that
/// one uses pointers and the other uses offsets to store data. All structures that uses ArrayRef
/// or StringRef are defined in `Index.def` so that they could have different definitions in
/// context.

/// Used to discribe the kind of relation between two symbols.
enum RelationKind : std::uint32_t {
    Invalid,
    Declaration,
    Definition,
    Reference,
    // Write Relation.
    Read,
    Write,
    Interface,
    Implementation,
    /// When target is a type definition of source, source is possible type or constructor.
    TypeDefinition,

    /// When target is a base class of source.
    Base,
    /// When target is a derived class of source.
    Derived,

    /// When target is a constructor of source.
    Constructor,
    /// When target is a destructor of source.
    Destructor,

    /// When target is a partial specialization of source.
    PartialSpecialization,
    /// When target is a full specialization of source.
    FullSpecialization,
    /// When target is an explicit instantiation of source.
    ImplicitInstantiation,

    // When target is a caller of source.
    Caller,
    // When target is a callee of source.
    Callee,
};

/// Represent a position in the source code, the line and column are 1-based.
struct Position {
    std::uint32_t line;
    std::uint32_t column;

    friend std::strong_ordering operator<=> (const Position&, const Position&) = default;
};

}  // namespace clice::index

namespace clice::index::in {

template <typename T>
using Ref = T;

using llvm::ArrayRef;
using llvm::StringRef;

#define MAKE_CLANGD_HAPPY
#include "Index.def"

inline SymbolID kindToSymbolID(std::uint64_t kind) {
    return SymbolID{kind, ""};
}

inline SymbolID USRToSymbolID(llvm::StringRef USR) {
    return SymbolID{llvm::hash_value(USR), USR};
}

inline std::strong_ordering operator<=> (const SymbolID& lhs, const SymbolID& rhs) {
    auto cmp = lhs.value <=> rhs.value;
    if(cmp != std::strong_ordering::equal) {
        return cmp;
    }
    return lhs.USR.compare(rhs.USR) <=> 0;
}

inline std::strong_ordering operator<=> (const Location& lhs, const Location& rhs) {
    auto cmp = lhs.file.compare(rhs.file);
    if(cmp != 0) {
        return cmp <=> 0;
    }
    return std::tuple{lhs.begin, lhs.end} <=> std::tuple{rhs.begin, rhs.end};
};

}  // namespace clice::index::in

namespace llvm {

using clice::index::in::kindToSymbolID;
using SymbolID = clice::index::in::SymbolID;
using Location = clice::index::in::Location;

template <>
struct DenseMapInfo<SymbolID> {
    inline static SymbolID getEmptyKey() {
        static SymbolID EMPTY_KEY = kindToSymbolID(std::numeric_limits<uint64_t>::max());
        return EMPTY_KEY;
    }

    inline static SymbolID getTombstoneKey() {
        static SymbolID TOMBSTONE_KEY = kindToSymbolID(std::numeric_limits<uint64_t>::max() - 1);
        return TOMBSTONE_KEY;
    }

    inline static llvm::hash_code getHashValue(const SymbolID& ID) {
        return ID.value;
    }

    inline static bool isEqual(const SymbolID& LHS, const SymbolID& RHS) {
        return LHS.value == RHS.value && LHS.USR == RHS.USR;
    }
};

template <>
struct DenseMapInfo<Location> {
    inline static Location getEmptyKey() {
        return Location{};
    }

    inline static Location getTombstoneKey() {
        return Location{.file = "Tombstone"};
    }

    inline static llvm::hash_code getHashValue(const Location& location) {
        return llvm::hash_combine(location.file, location.begin.line, location.begin.column, location.end.line, location.end.column);
    }

    inline static bool isEqual(const Location& LHS, const Location& RHS) {
        return LHS <=> RHS == std::strong_ordering::equal;
    }
};

}  // namespace llvm

namespace clice::index::out {

/// Because `SymbolID` and `Location` are duplicate referenced by `Relation`, `Symbol` and `Occurrence`,
/// To save space, we use offsets to index them.
template <typename T>
struct Ref {
    std::uint32_t offset;
};

template <typename T>
struct ArrayRef {
    std::uint32_t offset;
    std::uint32_t length;
};

using StringRef = ArrayRef<char>;

#define MAKE_CLANGD_HAPPY
#include "Index.def"

}  // namespace clice::index::out

