#include <Support/Enum.h>

namespace clice::index {

enum class RelationKinds : uint32_t {
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

    // When target is a caller of source.
    Caller,
    // When target is a callee of source.
    Callee,
};

/// A bit field enum to describe the kind of relation between two symbols.
struct RelationKind : enum_type<RelationKinds, true> {
    using enum RelationKinds;
    using enum_type::enum_type;
    using enum_type::operator=;
};

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

    friend constexpr auto operator<=> (const Ref& lhs, const Ref& rhs) = default;
};

struct LocationRef : Ref<LocationRef> {};

struct SymOrLocRef : Ref<SymOrLocRef> {};

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

    static bool isSymbol(const Relation& relation) {
        auto kind = relation.kind;
        return kind.is(RelationKinds::Interface) || kind.is(RelationKinds::Implementation) ||
               kind.is(RelationKinds::TypeDefinition) || kind.is(RelationKinds::Base) ||
               kind.is(RelationKinds::Derived) || kind.is(RelationKinds::Constructor) ||
               kind.is(RelationKinds::Destructor) || kind.is(RelationKinds::Caller) ||
               kind.is(RelationKinds::Callee);
    }
};

}  // namespace clice::index
