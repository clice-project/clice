#include "Index/Index.h"
#include "Support/Binary.h"
#include "Support/Format.h"

namespace clice::index {

// RelationKind SymbolIndex::Relation::kind() const {
//     auto relation = binary::Proxy<memory::Relation>{base, data};
//     return relation->kind;
// }

/// FIXME: check relation ...

// std::optional<LocalSourceRange> SymbolIndex::Relation::range() const {
//     binary::Proxy<memory::Relation> relation{base, data};
//     binary::Proxy<memory::SymbolIndex> index{base, base};
//
//     if(kind().is_one_of(RelationKind::Definition,
//                         RelationKind::Declaration,
//                         RelationKind::Reference,
//                         RelationKind::WeakReference)) {
//         return index.get<"ranges">()[relation->data];
//     } else if(kind().is_one_of(RelationKind::Caller, RelationKind::Callee)) {
//         return index.get<"ranges">()[relation->data1];
//     }
//
//     return {};
// }

// std::optional<LocalSourceRange> SymbolIndex::Relation::symbolRange() const {
//     binary::Proxy<memory::Relation> relation{base, data};
//     binary::Proxy<memory::SymbolIndex> index{base, base};
//
//     if(kind().is_one_of(RelationKind::Definition, RelationKind::Declaration)) {
//         return index.get<"ranges">()[relation->data1];
//     }
//
//     return {};
// }

// std::optional<SymbolIndex::Symbol> SymbolIndex::Relation::symbol() const {
//     binary::Proxy<memory::Relation> relation{base, data};
//     binary::Proxy<memory::SymbolIndex> index{base, base};
//
//     if(kind().is_one_of(RelationKind::Interface,
//                         RelationKind::Implementation,
//                         RelationKind::TypeDefinition,
//                         RelationKind::Base,
//                         RelationKind::Derived,
//                         RelationKind::Constructor,
//                         RelationKind::Destructor,
//                         RelationKind::Caller,
//                         RelationKind::Callee)) {
//         auto symbol = index.get<"symbols">()[relation->data];
//         return Symbol{base, symbol.data};
//     }
//
//     return {};
// }

// uint64_t SymbolIndex::SymbolID::id() const {
//     binary::Proxy<memory::Symbol> symbol{base, data};
//     return symbol.get<"id">();
// }
//
// llvm::StringRef SymbolIndex::SymbolID::name() const {
//     binary::Proxy<memory::Symbol> symbol{base, data};
//     return symbol.get<"name">().as_string();
// }
//
// SymbolKind SymbolIndex::Symbol::kind() const {
//     binary::Proxy<memory::Symbol> symbol{base, data};
//     return symbol.get<"kind">();
// }
//
// ArrayView<SymbolIndex::Relation> SymbolIndex::Symbol::relations() const {
//     binary::Proxy<memory::Symbol> symbol{base, data};
//     auto relations = symbol.get<"relations">().as_array();
//     return ArrayView<SymbolIndex::Relation>{
//         base,
//         relations.data(),
//         relations.size(),
//         sizeof(decltype(relations)::value_type),
//     };
// }

// LocalSourceRange SymbolIndex::Occurrence::range() const {
//     binary::Proxy<memory::Occurrence> occurrence{base, data};
//     binary::Proxy<memory::SymbolIndex> index{base, base};
//     assert(occurrence->location.valid() && "Invalid occurrence reference");
//     return index.get<"ranges">()[occurrence->location];
// }
//
// SymbolIndex::Symbol SymbolIndex::Occurrence::symbol() const {
//     binary::Proxy<memory::Occurrence> occurrence{base, data};
//     binary::Proxy<memory::SymbolIndex> index{base, base};
//     assert(occurrence->symbol.valid() && "Invalid symbol reference");
//     return Symbol{base, index.get<"symbols">()[occurrence->symbol].data};
// }
//
// llvm::StringRef SymbolIndex::path() const {
//     binary::Proxy<memory::SymbolIndex> index{base, base};
//     return index.get<"path">().as_string();
// }

/// Locate symbols at the given position.
// void SymbolIndex::locateSymbols(uint32_t position,
//                                 llvm::SmallVectorImpl<SymbolIndex::Symbol>& symbols) const {
//     binary::Proxy<memory::SymbolIndex> index{base, base};
//
//     auto ranges = index.get<"ranges">().as_array();
//     auto occurrences = index.get<"occurrences">().as_array();
//
//     auto iter = std::ranges::lower_bound(occurrences, position, {}, [&](const auto& occurrence) {
//         return ranges[occurrence.location].end;
//     });
//
//     for(; iter != occurrences.end(); ++iter) {
//         auto occurrence = *iter;
//         if(ranges[occurrence.location].begin > position) {
//             break;
//         }
//
//         symbols.emplace_back(
//             SymbolIndex::Symbol{base, index.get<"symbols">()[occurrence.symbol].data});
//     }
// }

// std::optional<SymbolIndex::Symbol> SymbolIndex::locateSymbol(uint64_t id,
//                                                              llvm::StringRef name) const {
//     binary::Proxy<memory::SymbolIndex> index{base, base};
//     auto symbols = index.get<"symbols">().as_array();
//     auto range = std::ranges::equal_range(symbols, id, {}, [&](const auto& symbol) {
//         return std::get<0>(symbol);
//     });
//
//     for(auto& symbol: range) {
//         binary::Proxy<memory::Symbol> symbolProxy{base, &symbol};
//         if(symbolProxy.get<"name">().as_string() == name) {
//             return SymbolIndex::Symbol{base, &symbol};
//         }
//     }
//
//     return {};
// }

}  // namespace clice::index

namespace clice::json {

// template <typename T>
// struct Serde<index::ArrayView<T>> {
//     static json::Value serialize(const index::ArrayView<T>& v) {
//         json::Array array;
//         for(const auto& element: v) {
//             array.push_back(json::serialize(element));
//         }
//         return array;
//     }
// };
//
// template <>
// struct Serde<index::SymbolIndex::Relation> {
//     static json::Value serialize(const index::SymbolIndex::Relation& v) {
//         json::Object object{
//             {"kind", json::serialize(v.kind().name())},
//         };
//
//         if(auto symbol = v.symbol()) {
//             object["target"] = json::serialize(symbol->id());
//         }
//
//         if(auto symbolRange = v.symbolRange()) {
//             object["symbolRange"] = json::serialize(*symbolRange);
//         }
//
//         if(auto range = v.range()) {
//             object["range"] = json::serialize(*range);
//         }
//
//         return object;
//     }
// };
//
// template <>
// struct Serde<index::SymbolIndex::Symbol> {
//     static json::Value serialize(const index::SymbolIndex::Symbol& v) {
//         return json::Object{
//             {"id",        json::serialize(v.id())         },
//             {"name",      json::serialize(v.name())       },
//             {"kind",      json::serialize(v.kind().name())},
//             {"relations", json::serialize(v.relations())  },
//         };
//     }
// };
//
// template <>
// struct Serde<index::SymbolIndex::Occurrence> {
//     static json::Value serialize(const index::SymbolIndex::Occurrence& v) {
//         return json::Object{
//             {"location", json::serialize(v.range()) },
//             {"symbol",   json::serialize(v.symbol())},
//         };
//     }
// };

// template <>
// struct Serde<index::SymbolIndex> {
//     static json::Value serialize(const index::SymbolIndex& v) {
//         return json::Object{
//             ///  {"symbols",     json::serialize(v.symbols())    },
//             /// {"occurrences", json::serialize(v.occurrences())},
//         };
//     }
// };

}  // namespace clice::json

namespace clice::index {

// json::Value SymbolIndex::toJSON() const {
//     return json::serialize(*this);
// }

}  // namespace clice::index
