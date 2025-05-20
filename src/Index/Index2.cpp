#include "Index/Index2.h"
#include "Support/Ranges.h"

namespace clice::index {

namespace memory2 {

/// Merge all elements from other into self. And update_context is invoked every time
/// when a element is inserted. The second argument is inserted `Contextual` in the
/// other, the first element is inserted element in the self, empty if the element
/// is new to self.
static void merge_elements(SymbolIndex& self, SymbolIndex& other, auto& update_context) {
    /// Merge symbols from other into self.
    for(auto& [symbol_id, symbol]: other.symbols) {
        auto [it, success] = self.symbols.try_emplace(symbol_id, std::move(symbol));
        auto& self_symbol = it->second;

        if(success) [[unlikely]] {
            /// If insert successfully, this is a new symbol and it means
            /// we need update all context states of this symbol.
            for(auto& relation: self_symbol.relations) {
                update_context(relation, Contextual(relation), true);
            }
            continue;
        }

        /// If self already has this symbol, try to merge all relations.
        for(auto& relation: symbol.relations) {
            auto [it, success] = self_symbol.relations.insert(relation);
            update_context(*it, Contextual(relation), success);
        }
    }

    for(auto& [range, occurrence_group]: other.occurrences) {
        auto [it, success] = self.occurrences.try_emplace(range, std::move(occurrence_group));
        auto& self_occurrence_group = it->second;

        if(success) [[unlikely]] {
            /// Insert successfully.
            for(auto& occurrence: self_occurrence_group) {
                update_context(occurrence, Contextual(occurrence), true);
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
                update_context(self_occurrence_group[i], Contextual(occurrence), false);
            } else {
                /// If not found insert new occurrence.
                auto& o = self_occurrence_group.emplace_back(occurrence);
                update_context(o, Contextual(occurrence), true);
            }
        }
    }
}

void SymbolIndex::quick_merge(this SymbolIndex& self, SymbolIndex& other) {
    assert(other.is_single_header_context() &&
           "quick merge could be only used for the index with single header context");

    /// We could make sure the other has only one header context.
    std::uint32_t new_hctx_id = self.alloc_hctx_id();

    Bitmap flag = self.erased_flag();
    bool is_new_cctx = false;
    std::uint32_t new_cctx_id = -1;

    llvm::SmallVector<std::uint32_t> visited_elem_ids;

    auto update_context = [&](Contextual& self_elem, Contextual other_elem, bool is_new) {
        std::uint32_t new_elem_id;

        if(is_new) {
            /// If this a new element, it means that the other index must introduce
            /// a new canonical context id, we don't need to do following calculation.
            is_new_cctx = true;

            if(new_cctx_id == -1) {
                new_cctx_id = self.alloc_cctx_id();
            }

            if(other_elem.is_dependent()) {
                new_elem_id = self.alloc_dependent_elem_id();
                self.dependent_elem_states[new_elem_id].set(new_cctx_id);
            } else {
                new_elem_id = self.alloc_independent_elem_id();
                self.independent_elem_states[new_elem_id].insert(new_hctx_id);
            }

            other_elem.set(new_elem_id);
            self_elem = other_elem;
        } else {
            if(self_elem.is_dependent()) {
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
    merge_elements(self, other, update_context);

    if(!is_new_cctx) {
        assert(new_cctx_id == -1 && flag.any());
        for(auto i = 0; i < self.max_cctx_id; i++) {
            if(!flag.test(i)) {
                continue;
            }

            if(self.cctx_element_refs[i] == other.cctx_element_refs.front()) {
                new_cctx_id = i;
                break;
            }
        }

        new_cctx_id = self.alloc_cctx_id();
    }

    /// In the end we set all visited element ids.
    for(auto id: visited_elem_ids) {
        self.dependent_elem_states[id].set(new_hctx_id);
    }
}

void SymbolIndex::merge(this SymbolIndex& self, SymbolIndex& other) {}

Symbol& SymbolIndex::getSymbol(std::uint64_t symbol_id) {
    assert(canonical_context_count() == 1 && "please use merge for multiple contexts");
    if(auto it = symbols.find(symbol_id); it != symbols.end()) {
        return it->second;
    }

    /// If not found, create a new symbol and return it.
    auto symbol_ref = symbols.size();
    auto [it, _] = symbols.try_emplace(symbol_id, Symbol{.id = symbol_id});
    return it->second;
}

void SymbolIndex::addRelation(Symbol& symbol, Relation relation, bool isDependent) {
    // assert(contexts.size() == 1 && "please use merge for multiple contexts");
    // assert(contexts[0].canonical_context_id == 0);

    /// relation.setDependent(isDependent);

    if(isDependent) {
        /// relation.addContext(0);
        /// element_context_id_ref_counts[0] += 1;
    } else {
        /// relation.set(independent_context_refs.size());
        /// independent_context_refs.emplace_back();
        /// independent_context_refs.back().insert(0);
    }

    symbol.relations.insert(relation);
}

void SymbolIndex::addOccurrence(LocalSourceRange range,
                                std::int64_t target_symbol,
                                bool isDependent) {
    // assert(contexts.size() == 1 && "please use merge for multiple contexts");
    // assert(contexts[0].canonical_context_id == 0);
    //
    // auto& targets = occurrences[range];
    // Occurrence occurrence;
    // occurrence.target_symbol = target_symbol;
    // occurrence.setDependent(isDependent);
    //
    // if(isDependent) {
    //     occurrence.addContext(0);
    //     element_context_id_ref_counts[0] += 1;
    // } else {
    //     occurrence.set(independent_context_refs.size());
    //     independent_context_refs.emplace_back();
    //     independent_context_refs.back().insert(0);
    // }
    //
    // targets.insert(occurrence);
}

}  // namespace memory2

}  // namespace clice::index
