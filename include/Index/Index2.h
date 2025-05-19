#pragma once

#include <deque>
#include <bitset>
#include <vector>
#include <variant>

#include "Contexts.h"
#include "Shared.h"
#include "AST/SymbolID.h"
#include "AST/SymbolKind.h"
#include "AST/RelationKind.h"
#include "AST/SourceCode.h"
#include "Support/Hash.h"

#include "llvm/ADT/BitVector.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SmallVector.h"

namespace clice::index::memory2 {

/// Represents an element that may belong to multiple translation-unit contexts.
struct Contextual {
    using ContextType = std::uint32_t;

    /// Bitmask encoding:
    /// - Lower bits [0..30) store the context value.
    /// - Highest bit (bit 31) marks the context as dependent or not.
    ContextType context_mask = 0;

    void setDependent(bool is_dependent) {
        constexpr std::uint32_t mask = 1u << 31;
        is_dependent ? (context_mask |= mask) : (context_mask &= ~mask);
    }

    /// Whether this element affects the same context judgement.
    /// i.e. headers that has different elements as different contexts.
    bool isDependent() const {
        constexpr std::uint32_t mask = 1u << (8 * sizeof(ContextType) - 1);
        return context_mask & mask;
    };

    void addContext(std::uint32_t context_id) {
        context_mask |= (ContextType(1) << context_id);
    }

    void removeContext(std::uint32_t context_id) {
        context_mask &= ~(ContextType(1) << context_id);
    }

    void set(std::uint32_t offset) {
        context_mask |= offset;
    }

    ContextType value() {
        constexpr std::uint32_t mask = 1u << (8 * sizeof(ContextType) - 1);
        return context_mask & ~mask;
    }
};

struct HeaderContext {
    /// The include offset of this header context.
    std::uint32_t include;

    /// The context id of this header context.
    std::uint32_t canonical_context_id;
};

struct Relation : Contextual {
    RelationKind kind;

    /// local source range of symbol index.
    LocalSourceRange range;

    union {
        std::int64_t target_symbol;
        LocalSourceRange definition_range;
    };
};

struct Symbol {
    std::uint64_t id;

    SymbolKind kind;

    /// Whether the symbol is a function local symbol.
    bool is_file_local = false;

    /// Whether the symbol is a function local symbol.
    bool is_function_local = false;

    std::string name;

    llvm::DenseSet<Relation> relations;
};

struct Occurrence : Contextual {
    /// Ref symbol.
    std::uint64_t target_symbol;
};

/// The core class used for managing the contexts.
struct ContextManager {
    /// All distinct HeaderContext instances.
    std::vector<HeaderContext> header_contexts;

    /// Erased header context ids for late reusing.
    std::vector<std::uint32_t> erased_header_context_ids;

    /// A map between source file path and its header contexts.
    /// Note that it is possible that the file includes the header multiple
    /// times and thus has multiple header contexts.
    llvm::StringMap<llvm::SmallVector<std::uint32_t, 4>> context_table;

    /// Multiple distinct header contexts can result in identical AST and LSP responses.
    /// This situation frequently occurs with self-contained header files. To address this,
    /// we introduce the concept of a canonical header context. Given M header contexts,
    /// they ultimately resolve into N unique canonical header contexts, where M >= N. Each of
    /// these canonical contexts possesses a distinct semantic interpretation. By leveraging
    /// canonical header contexts, we not only avoid storing redundant indexing data, thereby
    /// saving disk space, but also enable rapid switching among different contextual views.
    std::uint32_t max_canonical_context_id = 0;

    /// The reference count of for each canonical context.
    /// [canonical context id -> count]
    std::vector<std::uint32_t> canonical_context_header_ref_counts;

    /// We don't perform immediate modification when removes header context,
    /// Store the erased context ids for reusing. So the true canonical context
    /// count is `max_canonical_context_id - canonical_context_erased_ids.size()`.
    std::vector<std::uint32_t> canonical_context_erased_ids;

    /// The states of all context-dependent elements, each bit in u32 represents
    /// whether this element occurs in corresponding id's canonical context.
    /// [element id -> canonical context mask]
    std::vector<llvm::BitVector> dependent_element_states;

    /// The The reference count of canonical context by elements, this is useful
    /// to determine whether two context are same when merges.
    /// [canonical context id -> count]
    std::vector<std::uint32_t> canonical_context_element_ref_counts;

    /// The states of all context-independent elements, it directly store the
    /// context id of the context rather than canonical context id.
    /// [element id -> header context ids]
    std::vector<llvm::DenseSet<std::uint32_t>> independent_element_states;

    void remove(llvm::StringRef file) {
        auto it = context_table.find(file);
        if(it == context_table.end()) {
            return;
        }

        llvm::SmallVector<std::uint32_t> new_erased_canonical_context_id;

        for(auto& header_context_id: it->second) {
            erased_header_context_ids.emplace_back(header_context_id);
            auto& header_context = header_contexts[header_context_id];

            auto& ref_count =
                canonical_context_header_ref_counts[header_context.canonical_context_id];
            assert(ref_count > 0);
            ref_count -= 1;
            if(ref_count == 0) {
                canonical_context_erased_ids.emplace_back(header_context.canonical_context_id);
                new_erased_canonical_context_id.emplace_back(header_context.canonical_context_id);
            }
        }

        if(new_erased_canonical_context_id.empty()) {
            return;
        }

        llvm::BitVector erased_flag;
        erased_flag.set();
        for(auto& n: new_erased_canonical_context_id) {
            erased_flag.reset(n);
        }

        for(auto& state: dependent_element_states) {}
    }
};

class SymbolIndex {
public:
    std::tuple<std::uint32_t, std::uint32_t> addContext(llvm::StringRef path,
                                                        std::uint32_t include);

    static index::Shared<std::unique_ptr<SymbolIndex>> build(ASTInfo& AST);

    void remove(llvm::StringRef path);

    void merge(this SymbolIndex& self, SymbolIndex& index);

    void remove(SymbolIndex& index);

    void update(SymbolIndex& index);

    Symbol& getSymbol(std::uint64_t symbol_id);

    void addRelation(Symbol& symbol, Relation relation, bool isDependent = true);

    void addOccurrence(LocalSourceRange range, std::int64_t target_symbol, bool isDependent = true);

    std::uint32_t unique_context_count() {
        return max_context_id - erased_context_ids.size();
    }

    std::uint32_t header_context_count() {
        return contexts.size() - erased_context_refs.size();
    }

    std::uint32_t file_count() {
        return contexts_table.size();
    }

    std::uint32_t symbol_count() {
        return symbols.size();
    }

    std::uint32_t occurrence_count() {
        return occurrences.size();
    }

public:
    /// Highest `context_id` allocated so far (monotonically increasing).
    std::uint32_t max_context_id = 0;

    /// Dense array holding every `HeaderContext`; indexed by `context_ref`.
    std::vector<HeaderContext> contexts;

    /// Pool of freed `context_id`s that can be recycled by `addContext`.
    std::deque<std::uint32_t> erased_context_ids;

    /// Pool of freed `context_ref`s that can be recycled by `addContext`.
    std::deque<std::uint32_t> erased_context_refs;

    /// Active reference count for each `context_id`. When the counter hits 0
    /// the corresponding ID is eligible for reuse and pushed to
    /// `erased_context_ids`.
    std::vector<std::uint32_t> header_context_id_ref_counts;

    /// Number of *dependent* elements bound to each `context_id`, used by the
    /// merge heuristics to detect equivalence with existing contexts.
    std::vector<std::uint32_t> element_context_id_ref_counts;

    /// For every *independent* element store the set of `context_ref`s in which
    /// the element appears.  Typical independent elements include template
    /// instantiations and macro expansions whose value does **not** affect the
    /// enclosing header context.
    std::vector<llvm::DenseSet<std::uint32_t>> independent_context_refs;

    /// Fast lookup table: header file path → vector of associated `context_ref`s
    /// (there may be multiple refs per physical file due to preprocessing).
    llvm::StringMap<llvm::SmallVector<std::uint32_t, 2>> contexts_table;

    /// Map from symbol ID to its stored Symbol object.
    llvm::DenseMap<std::uint64_t, Symbol> symbols;

    /// Map from source range to the set of symbol occurrences at that range.
    llvm::DenseMap<LocalSourceRange, llvm::DenseSet<Occurrence>> occurrences;
};

}  // namespace clice::index::memory2

template <typename... Ts>
unsigned dense_hash(const Ts&... ts) {
    return llvm::DenseMapInfo<std::tuple<Ts...>>::getHashValue(std::tuple{ts...});
}

template <>
struct llvm::DenseMapInfo<clice::LocalSourceRange> {
    using Range = clice::LocalSourceRange;

    inline static Range getEmptyKey() {
        return Range{std::uint32_t(-1), std::uint32_t(0)};
    }

    inline static Range getTombstoneKey() {
        return Range{std::uint32_t(0), std::uint32_t(-1)};
    }

    static unsigned getHashValue(const Range& range) {
        return dense_hash(range.begin, range.end);
    }

    static bool isEqual(const Range& LHS, const Range& RHS) {
        return LHS == RHS;
    }
};

template <>
struct llvm::DenseMapInfo<clice::index::memory2::Occurrence> {
    using Occurrence = clice::index::memory2::Occurrence;
    using Base = llvm::DenseMapInfo<std::uint64_t>;

    inline static Occurrence getEmptyKey() {
        return Occurrence{.target_symbol = Base::getEmptyKey()};
    }

    inline static Occurrence getTombstoneKey() {
        return Occurrence{.target_symbol = Base::getTombstoneKey()};
    }

    static unsigned getHashValue(const Occurrence& occurrence) {
        return dense_hash(occurrence.target_symbol);
    }

    static bool isEqual(const Occurrence& lhs, const Occurrence& rhs) {
        return lhs.target_symbol == rhs.target_symbol;
    }
};

template <>
struct llvm::DenseMapInfo<clice::index::memory2::Relation> {
    using Relation = clice::index::memory2::Relation;

    inline static Relation getEmptyKey() {
        return Relation{
            .kind = clice::RelationKind(),
            .range = clice::LocalSourceRange{std::uint32_t(-1), std::uint32_t(0)},
            .target_symbol = 0,
        };
    }

    inline static Relation getTombstoneKey() {
        return Relation{
            .kind = clice::RelationKind(),
            .range = clice::LocalSourceRange{std::uint32_t(0), std::uint32_t(-1)},
            .target_symbol = 0,
        };
    }

    static unsigned getHashValue(const Relation& relation) {
        return dense_hash(relation.kind.value(),
                          relation.range.begin,
                          relation.range.end,
                          relation.target_symbol);
    }

    static bool isEqual(const Relation& lhs, const Relation& rhs) {
        return lhs.kind == rhs.kind && lhs.range == rhs.range &&
               lhs.target_symbol == rhs.target_symbol;
    }
};
