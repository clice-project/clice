#pragma once

#include "RawIndex.h"

namespace clice::index::memory {

/// HeaderIndex store extra information to merge raw index from different header contexts.
class HeaderIndex : public RawIndex {
public:
    std::uint32_t file_count() {
        return header_contexts.size();
    }

    /// The count of active header contexts in this index.
    std::uint32_t header_context_count() {
        return max_hctx_id - erased_hctx_ids.size();
    }

    /// The count of active canonical contexts in this index.
    std::uint32_t canonical_context_count() {
        return max_cctx_id - erased_cctx_ids.size();
    }

    /// Whether this contexts has only one single context.
    bool is_single_header_context() {
        return max_hctx_id == 1 && erased_hctx_ids.empty();
    }

    auto erased_flag() {
        Bitmap map;
        map.set();
        for(auto cctx_id: erased_cctx_ids) {
            map.reset(cctx_id);
        }
        return map;
    }

    /// Get a new header context id.
    std::uint32_t alloc_hctx_id();

    /// Get a new canonical context id.
    std::uint32_t alloc_cctx_id();

    std::uint32_t alloc_dependent_elem_id() {
        auto id = dependent_elem_states.size();
        dependent_elem_states.emplace_back(false);
        return id;
    }

    std::uint32_t alloc_independent_elem_id() {
        auto id = independent_elem_states.size();
        independent_elem_states.emplace_back();
        return id;
    }

    struct HeaderContext {
        /// The include location id of this header context.
        std::uint32_t include;

        /// The header context id of this header context.
        std::uint32_t hctx_id;

        /// The canonical context id of this header context.
        std::uint32_t cctx_id;
    };

    void remove(this HeaderIndex& self, llvm::StringRef path);

    HeaderContext add_context(llvm::StringRef path, std::uint32_t include) {
        assert(!merged && "");
        auto& context = header_contexts[path].emplace_back();
        context.include = include;
        context.cctx_id = alloc_cctx_id();
        context.hctx_id = alloc_hctx_id();
        return context;
    }

    HeaderContext
        merge(this HeaderIndex& self, llvm::StringRef path, std::uint32_t include, RawIndex& raw);

public:
    bool merged = false;

    /// The max header context id.
    std::uint32_t max_hctx_id = 0;

    /// The max canonical context id.
    std::uint32_t max_cctx_id = 0;

    /// The erased header context id. if a header context is erased,
    /// we add its id for later reusing.
    std::deque<std::uint32_t> erased_hctx_ids;

    /// Same as above but for canonical context id.
    std::deque<std::uint32_t> erased_cctx_ids;

    /// A map between source file path and its header contexts.
    llvm::StringMap<llvm::SmallVector<HeaderContext>> header_contexts;

    /// A map between canonical context id and corresponding ref counts
    /// referenced by header contexts.
    llvm::SmallVector<std::uint32_t> cctx_hctx_refs;

    /// A map between canonical context id and corresponding ref counts
    /// referenced by contextual elements.
    llvm::SmallVector<std::uint32_t> cctx_element_refs;

    using Bitmap = std::bitset<64>;  /// use llvm::BitVector?

    /// A map between dependent element id and its state, for dependent element
    /// we use bitmap to store states. Each bit in bitmap represents whether
    /// this element occurs in corresponding canonical context id.
    llvm::SmallVector<Bitmap> dependent_elem_states;

    /// A map between independent element id and its state, for independent element
    /// we directly store the header context ids that it occurs in.
    std::vector<llvm::DenseSet<std::uint32_t>> independent_elem_states;
};

}  // namespace clice::index::memory

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
struct DenseMapInfo<clice::index::memory::Relation> {
    using R = clice::index::memory::Relation;

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
