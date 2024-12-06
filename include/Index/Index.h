#pragma once

#include "Basic/Location.h"
#include "Compiler/Semantic.h"
#include "Support/Support.h"

namespace clice::index {

template <typename T>
struct Ref {
    std::uint32_t offset = std::numeric_limits<std::uint32_t>::max();

    bool isValid() const {
        return offset != std::numeric_limits<std::uint32_t>::max();
    }

    bool isInvalid() const {
        return !isValid();
    }

    explicit operator bool () const {
        return isValid();
    }
};

struct FileRef : Ref<FileRef> {};

struct SymbolRef : Ref<SymbolRef> {};

struct LocationRef : Ref<LocationRef> {};

struct SymOrLocRef : Ref<SymOrLocRef> {};

/// Beacuse we decide to support header context, so just a file cannot represent a symbol location
/// well. So we define a new struct `Location` to represent the location of a symbol.
struct Location {
    /// The index of the file in the `Index::files`.
    FileRef file;

    /// Source code range of the location.
    proto::Range range;
};

struct Occurrence {
    /// The index of the location in `Index::locations`.
    LocationRef location;

    /// The index of the symbol in `Index::symbols`.
    SymbolRef symbol;
};

struct Relation {
    /// The kind of the relation.
    RelationKind kind;

    /// The index of location in `Index::locations`.
    LocationRef location;

    /// Additional information based on the `Relation::kind`. For `Declaration` and `Definition`,
    /// this is the source range of the whole entity(`Relation::location` is just the range of
    /// symbol name). For kinds whose target symbol is different from the source symbol, e.g.
    /// `TypeDefinition` and `Base`, this is the index of target symbol in `Index::symbols`.
    SymOrLocRef symOrLoc;
};

};  // namespace clice::index
