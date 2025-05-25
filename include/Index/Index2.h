#pragma once

#include <deque>
#include <bitset>
#include <vector>
#include <variant>

#include "Shared.h"
#include "Contexts.h"
#include "AST/SymbolID.h"
#include "AST/SymbolKind.h"
#include "AST/RelationKind.h"
#include "AST/SourceCode.h"

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SmallVector.h"

namespace clice::index::memory2 {

using SymbolID = std::uint64_t;

using SourceRange = LocalSourceRange;

struct Relation {
    Contextual ctx;
    RelationKind kind;

    /// The range of this relation.
    SourceRange range;

    union {
        SymbolID target_symbol;
        SourceRange definition_range;
    };
};

struct Symbol {
    /// The symbol id.
    SymbolID id;

    /// The symbol kind.
    SymbolKind kind;

    /// Whether this symbol is not visible to other tu.
    bool is_tu_local = false;

    /// Whether this symbol is defined in function scope.
    bool is_function_local = false;

    /// The symbol name.
    std::string name;

    /// All relations of this symbol.
    llvm::DenseSet<Relation> relations;
};

struct Occurrence {
    Contextual ctx;
    SymbolID target_symbol;
};

/// For most of symbol occurrence, it has only one corresponding symbol.
using OccurrenceGroup = llvm::SmallVector<Occurrence, 1>;

class SymbolIndex : public Contexts {
public:
    static index::Shared<std::unique_ptr<SymbolIndex>> build(ASTInfo& AST);

    Symbol& getSymbol(std::uint64_t symbol_id);

    HeaderContext add_context(llvm::StringRef path, std::uint32_t include) {
        assert(!merged && "");
        auto& context = header_contexts[path].emplace_back();
        context.include = include;
        context.cctx_id = alloc_cctx_id();
        context.hctx_id = alloc_hctx_id();
        return context;
    }

    void addRelation(Symbol& symbol, Relation relation, bool is_dependent = true);

    void addOccurrence(LocalSourceRange range,
                       std::int64_t target_symbol,
                       bool is_dependent = true);

    HeaderContext merge(this SymbolIndex& self, SymbolIndex& other);
        
private:
    /// Merge another index into this. Most of header file is actually
    /// self contained file and has only one canonical context. This
    /// is a fast path for it.
    HeaderContext quick_merge(this SymbolIndex& self, SymbolIndex& other);

    /// Merge another index into this, this could handle even though
    /// another has multiple canonical context. But of course slow than
    /// the fast path.
    /// TODO: This function hasn't been implemented.
    void slow_merge(this SymbolIndex& self, SymbolIndex& other);

public:
    /// Whether this has been merged with other files.
    bool merged = false;

    /// All symbols in this index.
    llvm::DenseMap<SymbolID, Symbol> symbols;

    /// All occurrences in this index.
    llvm::DenseMap<SourceRange, OccurrenceGroup> occurrences;
};

}  // namespace clice::index::memory2

namespace llvm {

template <typename... Ts>
unsigned dense_hash(const Ts&... ts) {
    return llvm::DenseMapInfo<std::tuple<Ts...>>::getHashValue(std::tuple{ts...});
}

template <>
struct DenseMapInfo<clice::LocalSourceRange> {
    using R = clice::LocalSourceRange;

    inline static R getEmptyKey() {
        return R(0, -1);
    }

    inline static R getTombstoneKey() {
        return R(-1, 0);
    }

    static auto getHashValue(const R& r) {
        return dense_hash(r.begin, r.end);
    }

    static bool isEqual(const R& lhs, const R& rhs) {
        return lhs == rhs;
    }
};

template <>
struct DenseMapInfo<clice::index::memory2::Relation> {
    using R = clice::index::memory2::Relation;

    inline static R getEmptyKey() {
        return R{
            .kind = clice::RelationKind(),
            .range = clice::LocalSourceRange(0, 0),
            .target_symbol = 0,
        };
    }

    inline static R getTombstoneKey() {
        return R{
            .kind = clice::RelationKind(),
            .range = clice::LocalSourceRange(-1, -1),
            .target_symbol = 0,
        };
    }

    /// Contextual doen't take part in hashing and equality.
    static auto getHashValue(const R& relation) {
        return dense_hash(relation.kind.value(),
                          relation.range.begin,
                          relation.range.end,
                          relation.target_symbol);
    }

    static bool isEqual(const R& lhs, const R& rhs) {
        return lhs.kind == rhs.kind && lhs.range == rhs.range &&
               lhs.target_symbol == rhs.target_symbol;
    }
};

}  // namespace llvm
