#include "BinarySymbolIndex.h"

namespace clice::index {

RelationKind SymbolIndex::Relation::kind() const {
    return static_cast<const binary::Relation*>(data)->kind;
}

Location SymbolIndex::Relation::range() const {
    auto index = static_cast<const binary::ProxyIndex*>(base);
    auto relation = static_cast<const binary::Relation*>(data);
    assert(relation->location != std::numeric_limits<uint32_t>::max() &&
           "Invalid location reference");
    return index->getLocations()[relation->location];
}

SymbolIndex::Symbol SymbolIndex::Relation::symbol() const {
    auto index = static_cast<const binary::ProxyIndex*>(base);
    auto relation = static_cast<const binary::Relation*>(data);
    assert(relation->extra != std::numeric_limits<uint32_t>::max() && "Invalid extra reference");
    return {base, &index->getSymbols()[relation->extra]};
}

uint64_t SymbolIndex::SymbolID::id() const {
    return static_cast<const binary::Symbol*>(data)->id;
}

llvm::StringRef SymbolIndex::SymbolID::name() const {
    auto index = static_cast<const binary::ProxyIndex*>(base);
    auto symbol = static_cast<const binary::Symbol*>(data);
    return index->getString(symbol->name);
}

SymbolKind SymbolIndex::Symbol::kind() const {
    return static_cast<const binary::Symbol*>(data)->kind;
}

ArrayView<SymbolIndex::Relation> SymbolIndex::Symbol::relations() const {
    auto index = static_cast<const binary::ProxyIndex*>(base);
    auto symbol = static_cast<const binary::Symbol*>(data);
    return index->getArrayView<SymbolIndex::Relation>(symbol->relations);
}

Location SymbolIndex::Occurrence::location() const {
    auto index = static_cast<const binary::ProxyIndex*>(base);
    auto occurrence = static_cast<const binary::Occurrence*>(data);
    assert(occurrence->location != std::numeric_limits<uint32_t>::max() &&
           "Invalid occurrence reference");
    return index->getLocations()[occurrence->location];
}

SymbolIndex::Symbol SymbolIndex::Occurrence::symbol() const {
    auto index = static_cast<const binary::ProxyIndex*>(base);
    auto occurrence = static_cast<const binary::Occurrence*>(data);
    assert(occurrence->symbol != std::numeric_limits<uint32_t>::max() &&
           "Invalid symbol reference");
    return {base, &index->getSymbols()[occurrence->symbol]};
}

ArrayView<SymbolIndex::Symbol> SymbolIndex::symbols() const {
    auto index = static_cast<const binary::ProxyIndex*>(base);
    return index->getArrayView<SymbolIndex::Symbol>(index->symbols);
}

ArrayView<SymbolIndex::Occurrence> SymbolIndex::occurrences() const {
    auto index = static_cast<const binary::ProxyIndex*>(base);
    return index->getArrayView<SymbolIndex::Occurrence>(index->occurrences);
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
        return json::Object{
            {"kind",   json::serialize(v.kind().name())},
            {"range",  json::serialize(v.range())      },
            {"symbol", json::serialize(v.symbol().id())},
        };
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
            {"location", json::serialize(v.location())},
            {"symbol",   json::serialize(v.symbol())  },
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
