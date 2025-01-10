#include "Index.h"

namespace clice::index {

struct SymbolIndexVisitor : binary::SymbolIndex {
    llvm::StringRef getString(binary::String string) const {
        return {reinterpret_cast<const char*>(this) + string.offset, string.size};
    }

    template <typename T>
    llvm::ArrayRef<T> getArray(binary::Array<T> array) const {
        return {reinterpret_cast<const T*>(reinterpret_cast<const char*>(this) + array.offset),
                array.size};
    }

    llvm::ArrayRef<binary::Symbol> getSymbols() const {
        return getArray(symbols);
    }

    llvm::ArrayRef<binary::Occurrence> getOccurrences() const {
        return getArray(occurrences);
    }

    llvm::ArrayRef<LocalSourceRange> getLocations() const {
        return getArray(ranges);
    }

    template <typename To, typename From>
    ArrayView<To> getArrayView(binary::Array<From> array) const {
        auto base = reinterpret_cast<const char*>(this);
        return {base, base + array.offset, array.size, sizeof(From)};
    }
};

RelationKind SymbolIndex::Relation::kind() const {
    return static_cast<const binary::Relation*>(data)->kind;
}

/// FIXME: check relation ...

std::optional<LocalSourceRange> SymbolIndex::Relation::range() const {
    auto index = static_cast<const SymbolIndexVisitor*>(base);
    auto relation = static_cast<const binary::Relation*>(data);
    if(kind().is_one_of(RelationKind::Definition,
                        RelationKind::Declaration,
                        RelationKind::Reference,
                        RelationKind::WeakReference)) {
        return index->getLocations()[relation->data[0]];
    } else if(kind().is_one_of(RelationKind::Caller, RelationKind::Callee)) {
        return index->getLocations()[relation->data[1]];
    }

    return {};
}

std::optional<LocalSourceRange> SymbolIndex::Relation::symbolRange() const {
    if(kind().is_one_of(RelationKind::Definition, RelationKind::Declaration) &&
       "Invalid relation kind") {
        auto index = static_cast<const SymbolIndexVisitor*>(base);
        auto relation = static_cast<const binary::Relation*>(data);
        return index->getLocations()[relation->data[1]];
    }

    return {};
}

std::optional<SymbolIndex::Symbol> SymbolIndex::Relation::symbol() const {
    if(kind().is_one_of(RelationKind::Interface,
                        RelationKind::Implementation,
                        RelationKind::TypeDefinition,
                        RelationKind::Base,
                        RelationKind::Derived,
                        RelationKind::Constructor,
                        RelationKind::Destructor,
                        RelationKind::Caller,
                        RelationKind::Callee)) {
        auto index = static_cast<const SymbolIndexVisitor*>(base);
        auto relation = static_cast<const binary::Relation*>(data);
        return SymbolIndex::Symbol{base, &index->getSymbols()[relation->data[0]]};
    }

    return {};
}

uint64_t SymbolIndex::SymbolID::id() const {
    return static_cast<const binary::Symbol*>(data)->id;
}

llvm::StringRef SymbolIndex::SymbolID::name() const {
    auto index = static_cast<const SymbolIndexVisitor*>(base);
    auto symbol = static_cast<const binary::Symbol*>(data);
    return index->getString(symbol->name);
}

SymbolKind SymbolIndex::Symbol::kind() const {
    return static_cast<const binary::Symbol*>(data)->kind;
}

ArrayView<SymbolIndex::Relation> SymbolIndex::Symbol::relations() const {
    auto index = static_cast<const SymbolIndexVisitor*>(base);
    auto symbol = static_cast<const binary::Symbol*>(data);
    return index->getArrayView<SymbolIndex::Relation>(symbol->relations);
}

LocalSourceRange SymbolIndex::Occurrence::range() const {
    auto index = static_cast<const SymbolIndexVisitor*>(base);
    auto occurrence = static_cast<const binary::Occurrence*>(data);
    assert(occurrence->location.valid() && "Invalid occurrence reference");
    return index->getLocations()[occurrence->location];
}

SymbolIndex::Symbol SymbolIndex::Occurrence::symbol() const {
    auto index = static_cast<const SymbolIndexVisitor*>(base);
    auto occurrence = static_cast<const binary::Occurrence*>(data);
    assert(occurrence->symbol.valid() && "Invalid symbol reference");
    return {base, &index->getSymbols()[occurrence->symbol]};
}

ArrayView<SymbolIndex::Symbol> SymbolIndex::symbols() const {
    auto index = static_cast<const SymbolIndexVisitor*>(base);
    return index->getArrayView<SymbolIndex::Symbol>(index->symbols);
}

ArrayView<SymbolIndex::Occurrence> SymbolIndex::occurrences() const {
    auto index = static_cast<const SymbolIndexVisitor*>(base);
    return index->getArrayView<SymbolIndex::Occurrence>(index->occurrences);
}

/// Locate symbols at the given position.
void SymbolIndex::locateSymbols(uint32_t position,
                                llvm::SmallVectorImpl<SymbolIndex::Symbol>& symbols) const {
    auto index = static_cast<const SymbolIndexVisitor*>(base);
    auto occurrences = index->getOccurrences();
    auto iter = std::ranges::lower_bound(occurrences, position, {}, [&](const auto& occurrence) {
        return index->getLocations()[occurrence.location].end;
    });

    for(; iter != occurrences.end(); ++iter) {
        auto occurrence = *iter;
        if(index->getLocations()[occurrence.location].begin > position) {
            break;
        }

        symbols.emplace_back(SymbolIndex::Symbol{base, &index->getSymbols()[occurrence.symbol]});
    }
}

/// Locate symbol with the given id(usually from another index).
SymbolIndex::Symbol SymbolIndex::locateSymbol(SymbolIndex::SymbolID ID) const {
    auto index = static_cast<const SymbolIndexVisitor*>(base);
    auto symbols = index->getSymbols();
    auto iter = std::ranges::lower_bound(symbols, ID.id(), {}, [&](const auto& symbol) {
        return symbol.id;
    });

    if(iter != symbols.end() && iter->id == ID.id() && index->getString(iter->name) == ID.name()) {
        return {base, &*iter};
    }

    return {};
}

}  // namespace clice::index

namespace clice::json {

template <typename T>
struct Serde<index::ArrayView<T>> {
    static json::Value serialize(const index::ArrayView<T>& v) {
        json::Array array;
        for(const auto& element: v) {
            array.push_back(json::serialize(element));
        }
        return array;
    }
};

template <>
struct Serde<index::SymbolIndex::Relation> {
    static json::Value serialize(const index::SymbolIndex::Relation& v) {
        json::Object object{
            {"kind", json::serialize(v.kind().name())},
        };

        if(auto symbol = v.symbol()) {
            object["target"] = json::serialize(symbol->id());
        }

        if(auto symbolRange = v.symbolRange()) {
            object["symbolRange"] = json::serialize(*symbolRange);
        }

        if(auto range = v.range()) {
            object["range"] = json::serialize(*range);
        }

        return object;
    }
};

template <>
struct Serde<index::SymbolIndex::Symbol> {
    static json::Value serialize(const index::SymbolIndex::Symbol& v) {
        return json::Object{
            {"id",        json::serialize(v.id())         },
            {"name",      json::serialize(v.name())       },
            {"kind",      json::serialize(v.kind().name())},
            {"relations", json::serialize(v.relations())  },
        };
    }
};

template <>
struct Serde<index::SymbolIndex::Occurrence> {
    static json::Value serialize(const index::SymbolIndex::Occurrence& v) {
        return json::Object{
            {"location", json::serialize(v.range()) },
            {"symbol",   json::serialize(v.symbol())},
        };
    }
};

template <>
struct Serde<index::SymbolIndex> {
    static json::Value serialize(const index::SymbolIndex& v) {
        return json::Object{
            {"symbols",     json::serialize(v.symbols())    },
            {"occurrences", json::serialize(v.occurrences())},
        };
    }
};

}  // namespace clice::json

namespace clice::index {

json::Value SymbolIndex::toJSON() const {
    return json::serialize(*this);
}

}  // namespace clice::index
