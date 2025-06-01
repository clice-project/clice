#include "Index/HeaderIndex.h"

namespace clice::index::memory {

std::uint32_t HeaderIndex::alloc_hctx_id() {
    std::uint32_t new_hctx_id;
    if(erased_hctx_ids.empty()) {
        new_hctx_id = max_hctx_id;
        max_hctx_id += 1;
    } else {
        new_hctx_id = erased_hctx_ids.front();
        erased_hctx_ids.pop_front();
    }
    return new_hctx_id;
}

std::uint32_t HeaderIndex::alloc_cctx_id() {
    std::uint32_t new_cctx_id;
    if(erased_cctx_ids.empty()) {
        new_cctx_id = max_cctx_id;
        max_cctx_id += 1;
        cctx_hctx_refs.emplace_back(1);
        cctx_element_refs.emplace_back(0);
    } else {
        new_cctx_id = erased_cctx_ids.front();
        erased_cctx_ids.pop_front();
        cctx_hctx_refs[new_cctx_id] = 1;
        cctx_element_refs[new_cctx_id] = 0;
    }
    return new_cctx_id;
}

void HeaderIndex::remove(this HeaderIndex& self, llvm::StringRef path) {
    auto it = self.header_contexts.find(path);

    /// If no such file, nothing to do.
    if(it == self.header_contexts.end()) {
        return;
    }

    llvm::SmallVector<std::uint32_t> erased_hctx_ids;
    llvm::SmallVector<std::uint32_t> erased_cctx_ids;

    for(auto& context: it->second) {
        erased_hctx_ids.push_back(context.hctx_id);
        self.erased_hctx_ids.push_back(context.hctx_id);

        auto cctx_id = context.cctx_id;
        auto& ref_count = self.cctx_hctx_refs[cctx_id];
        assert(ref_count > 0);

        /// If the ref count of the canonical context id drops to 0,
        /// we need to delete it.
        ref_count -= 1;
        if(ref_count == 0) {
            erased_cctx_ids.push_back(cctx_id);
            self.erased_cctx_ids.push_back(cctx_id);
            self.cctx_element_refs[cctx_id] = 0;
        }
    }

    self.header_contexts.erase(it);

    /// Remove all refs to this header context id.
    for(auto& state: self.independent_elem_states) {
        for(auto hctx_id: erased_hctx_ids) {
            state.erase(hctx_id);
        }
    }

    /// Remove all refs to this canonical context id.
    Bitmap erased_flag = self.erased_flag();

    for(auto& state: self.dependent_elem_states) {
        state &= erased_flag;
    }
}

/// Merge all elements from other into self. And update_context is invoked every time
/// when a element is inserted. The second argument is inserted `Contextual` in the
/// other, the first element is inserted element in the self, empty if the element
/// is new to self.
static void merge_elements(HeaderIndex& self, RawIndex& raw, auto& update_context) {
    /// Merge symbols from other into self.
    for(auto& [symbol_id, symbol]: raw.symbols) {
        auto [it, success] = self.symbols.try_emplace(symbol_id, std::move(symbol));
        auto& self_symbol = it->second;

        if(success) [[unlikely]] {
            /// If insert successfully, this is a new symbol and it means
            /// we need update all context states of this symbol.
            for(auto& relation: self_symbol.relations) {
                update_context(relation.ctx, relation.ctx.is_dependent(), true);
            }
            continue;
        }

        /// If self already has this symbol, try to merge all relations.
        for(auto& relation: symbol.relations) {
            auto [it, success] = self_symbol.relations.insert(relation);
            update_context(it->ctx, relation.ctx.is_dependent(), success);
        }
    }

    for(auto& [range, occurrence_group]: raw.occurrences) {
        auto [it, success] = self.occurrences.try_emplace(range, std::move(occurrence_group));
        auto& self_occurrence_group = it->second;

        if(success) [[unlikely]] {
            /// Insert successfully.
            for(auto& occurrence: self_occurrence_group) {
                update_context(occurrence.ctx, occurrence.ctx.is_dependent(), true);
            }
            continue;
        }

        for(auto& occurrence: occurrence_group) {
            auto i = 0;

            /// In most of cases, there is only one element in the group.
            /// So don't worry about the performance.
            for(auto& self_occurrence: self_occurrence_group) {
                if(occurrence.target_symbol == self_occurrence.target_symbol) {
                    break;
                }
                i += 1;
            }

            if(i != self_occurrence_group.size()) {
                update_context(self_occurrence_group[i].ctx, occurrence.ctx.is_dependent(), false);
            } else {
                /// If not found insert new occurrence.
                auto& o = self_occurrence_group.emplace_back(occurrence);
                update_context(o.ctx, occurrence.ctx.is_dependent(), true);
            }
        }
    }
}

auto HeaderIndex::merge(this HeaderIndex& self,
                        llvm::StringRef path,
                        std::uint32_t include,
                        RawIndex& raw) -> HeaderContext {
    /// We could make sure the other has only one header context.
    std::uint32_t new_hctx_id = self.alloc_hctx_id();

    Bitmap flag = self.erased_flag();
    bool is_new_cctx = false;
    std::uint32_t new_cctx_id = -1;

    llvm::SmallVector<std::uint32_t> visited_elem_ids;

    /// TODO: simplify the logic of update context.

    std::uint32_t old_elements_refs = 0;

    auto update_context = [&](Contextual& self_elem, bool is_dependent, bool is_new) {
        std::uint32_t new_elem_id;

        if(is_new) {
            /// If this a new element, it means that the other index must introduce
            /// a new canonical context id, we don't need to do following calculation.
            is_new_cctx = true;

            if(new_cctx_id == -1) {
                new_cctx_id = self.alloc_cctx_id();
            }

            if(is_dependent) {
                old_elements_refs += 1;
                new_elem_id = self.alloc_dependent_elem_id();
                self.dependent_elem_states[new_elem_id].set(new_cctx_id);
            } else {
                new_elem_id = self.alloc_independent_elem_id();
                self.independent_elem_states[new_elem_id].insert(new_hctx_id);
            }

            self_elem = Contextual::from(is_dependent, new_elem_id);
        } else {
            if(self_elem.is_dependent()) {
                old_elements_refs += 1;
                if(is_new_cctx) {
                    /// If this element is not new, but we already make sure the context is new
                    /// add its context.
                    self.dependent_elem_states[self_elem.offset()].set(new_cctx_id);
                } else {
                    /// If this element is not new and we still cannot make sure whether this is
                    /// new canonical context.
                    flag &= self.dependent_elem_states[self_elem.offset()];
                    visited_elem_ids.emplace_back(self_elem.offset());
                    if(flag.none()) {
                        is_new_cctx = true;
                    }
                }
            } else {
                self.independent_elem_states[self_elem.offset()].insert(new_hctx_id);
            }
        }
    };

    /// Merge all elements from other into self and calculate the bitmap state.
    merge_elements(self, raw, update_context);

    if(!is_new_cctx) {
        assert(new_cctx_id == -1 && flag.any());
        for(auto i = 0; i < self.max_cctx_id; i++) {
            if(!flag.test(i)) {
                continue;
            }

            if(self.cctx_element_refs[i] == old_elements_refs) {
                new_cctx_id = i;
                break;
            }
        }
    }

    if(new_cctx_id == -1) {
        new_cctx_id = self.alloc_cctx_id();
        is_new_cctx = true;
    }

    if(is_new_cctx) {
        /// In the end we set all visited element ids.
        for(auto id: visited_elem_ids) {
            self.dependent_elem_states[id].set(new_cctx_id);
        }
        self.cctx_element_refs[new_cctx_id] = old_elements_refs;
    }

    return self.header_contexts[path].emplace_back(HeaderContext{
        .include = include,
        .hctx_id = new_hctx_id,
        .cctx_id = new_cctx_id,
    });
}

}  // namespace clice::index::memory
