#pragma once

#include <bitset>
#include <cstdint>
#include <deque>
#include <vector>

#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/BitVector.h"

namespace clice::index {

using ElementID = std::uint32_t;

struct Contextual {
    ElementID element_id;

    bool is_dependent() {
        return true;
    }

    void set(std::uint32_t offset) {}

    std::uint32_t offset() {}
};

/// A header context could be represented by file:include.
/// In the following context, hctx means "header context" and cctx means
/// "canonical context". So hcid is header context id and ccid is
/// canonical context id.
class Contexts {
public:
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

    void add_context(llvm::StringRef path, std::uint32_t include);

    /// Get a new header context id.
    std::uint32_t alloc_hctx_id();

    /// Get a new canonical context id.
    std::uint32_t alloc_cctx_id();

    ElementID alloc_dependent_elem_id() {
        auto id = dependent_elem_states.size();
        dependent_elem_states.emplace_back(false);
        return id;
    }

    ElementID alloc_independent_elem_id() {
        auto id = independent_elem_states.size();
        independent_elem_states.emplace_back();
        return id;
    }

    void remove(this Contexts& self, llvm::StringRef path);

protected:
    /// The max header context id.
    std::uint32_t max_hctx_id = 0;

    /// The max canonical context id.
    std::uint32_t max_cctx_id = 0;

    /// The erased header context id. if a header context is erased,
    /// we add its id for later reusing.
    std::deque<std::uint32_t> erased_hctx_ids;

    /// Same as above but for canonical context id.
    std::deque<std::uint32_t> erased_cctx_ids;

    struct HeaderContext {
        /// The include location id of this header context.
        std::uint32_t include;

        /// The header context id of this header context.
        std::uint32_t hctx_id;

        /// The canonical context id of this header context.
        std::uint32_t cctx_id;
    };

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

}  // namespace clice::index
