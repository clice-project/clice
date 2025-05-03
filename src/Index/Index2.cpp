#include "Index/Index2.h"

namespace clice::index {

namespace memory2 {

std::tuple<std::uint32_t, std::uint32_t> SymbolIndex::addContext(llvm::StringRef path,
                                                                 std::uint32_t include) {
    std::uint32_t context_id;
    if(!erased_context_ids.empty()) {
        /// Reuse erased context id.
        context_id = erased_context_ids.front();
        erased_context_ids.pop_front();
        header_context_id_ref_counts[context_id] += 1;
    } else {
        /// Create new context id.
        context_id = max_context_id;
        header_context_id_ref_counts.emplace_back(1);
        element_context_id_ref_counts.emplace_back(0);
        max_context_id += 1;
    }

    std::uint32_t context_ref;
    if(!erased_context_refs.empty()) {
        /// Reuse erased context.
        context_ref = erased_context_refs.front();
        erased_context_refs.pop_front();
    } else {
        /// Create new context.
        context_ref = contexts.size();
        contexts.emplace_back();
    }

    contexts[context_ref].include = include;
    contexts[context_ref].context_id = context_id;

    /// Update the context table.
    auto it = contexts_table.find(path);
    if(it != contexts_table.end()) {
        for(auto& context_ref: it->second) {
            assert(contexts[context_ref].include != include &&
                   "cannot add an existed context, remove first");
        }

        it->second.emplace_back(context_ref);
    } else {
        contexts_table[path].emplace_back(context_ref);
    }

    return std::tuple{context_id, context_ref};
}

void SymbolIndex::remove(llvm::StringRef path) {
    auto it = contexts_table.find(path);
    if(it == contexts_table.end()) {
        return;
    }

    llvm::SmallVector<std::uint32_t> buffer;

    for(auto context_ref: it->second) {
        erased_context_refs.emplace_back(context_ref);
        auto& context = contexts[context_ref];
        auto& ref_counts = header_context_id_ref_counts[context.context_id];
        assert(ref_counts > 0 && "unexpected error");
        ref_counts -= 1;
        if(ref_counts == 0) {
            erased_context_ids.emplace_back(context.context_id);
            element_context_id_ref_counts[context.context_id] = 0;
        }

        buffer.emplace_back(context_ref);
    }

    /// Remove all refs of these header contexts.
    for(auto& refs: independent_context_refs) {
        for(auto& context_ref: buffer) {
            refs.erase(context_ref);
        }
    }

    contexts_table.erase(it);
}

static void mergeElements(auto& update_context, SymbolIndex& self, SymbolIndex& other) {
    /// Merge symbols.
    for(auto& [symbol_id, new_symbol]: other.symbols) {
        auto it = self.symbols.find(symbol_id);
        if(it != self.symbols.end()) {
            /// If we already the symbol, try to merge the two symbol.
            auto& symbol = it->second;

            /// Merge all the relations.
            for(auto& new_relation: new_symbol.relations) {
                auto it = symbol.relations.find(new_relation);
                if(it != symbol.relations.end()) {
                    /// If the relation is found, up the relation context.
                    update_context(*it, false);
                    continue;
                }

                /// If not such relation, update its context and insert.
                update_context(new_relation, true);
                symbol.relations.insert(new_relation);
            }
        } else {
            /// set context ...
            for(auto& new_relation: new_symbol.relations) {
                update_context(new_relation, true);
            }

            /// If there isn't corresponding symbol, add it.
            self.symbols[symbol_id] = std::move(new_symbol);
        }
    }

    /// Merge occurrences.
    for(auto& [range, new_occurrences]: other.occurrences) {
        auto it = self.occurrences.find(range);
        if(it != self.occurrences.end()) {
            auto& occurrences = it->second;

            for(auto& new_occurrence: new_occurrences) {
                auto it = occurrences.find(new_occurrence);
                if(it != occurrences.end()) {
                    update_context(*it, false);
                    continue;
                }

                update_context(new_occurrence, true);
                occurrences.insert(new_occurrence);
            }
        } else {
            /// set context ...
            for(auto& new_occurrence: new_occurrences) {
                update_context(new_occurrence, true);
            }

            /// If there isn't corresponding symbol, add it.
            self.occurrences.try_emplace(range, std::move(new_occurrences));
        }
    }
}

void SymbolIndex::merge(this SymbolIndex& self, SymbolIndex& other) {
    assert(other.contexts_table.size() == 1);
    auto& [path, context_refs] = *other.contexts_table.begin();
    assert(context_refs.size() == 1);
    auto& new_context = other.contexts[context_refs[0]];
    auto [new_context_id, new_context_ref] = self.addContext(path, new_context.include);

    /// Whether we can make sure this new index introduces
    /// a new header context, if so we don't need two traverse
    /// the inde twice.
    bool needResolve = true;

    std::bitset<32> flag;
    for(auto i = 0; i < self.max_context_id; i++) {
        flag.set(i);
    }

    for(auto erased_id: self.erased_context_ids) {
        flag.reset(erased_id);
    }

    auto update_context = [&](Contextual& ctx, bool is_new) {
        if(ctx.isDependent()) {
            if(!is_new && needResolve) {
                flag &= ctx.value();
            } else {
                needResolve = false;
            }

            ctx.addContext(new_context_id);
            self.element_context_id_ref_counts[new_context_id] += 1;
        } else {
            if(is_new) {
                ctx.context_mask = self.independent_context_refs.size();
                self.independent_context_refs.emplace_back();
                self.independent_context_refs.back().insert(new_context_ref);
            } else {
                auto offset = ctx.value();
                self.independent_context_refs[offset].insert(new_context_ref);
            }
        }
    };

    /// Merge all elements.
    mergeElements(update_context, self, other);

    /// If we make sure the new index introduces a new context, directly return.
    if(needResolve && flag.any()) {
        llvm::SmallVector<std::uint32_t> ids;

        for(auto i = 0; i < self.max_context_id; i++) {
            auto& ref_counts = self.element_context_id_ref_counts;
            if(i != new_context_id && flag.test(i) && ref_counts[i] == ref_counts[new_context_id]) {
                ids.emplace_back(i);
            }
        }

        if(ids.size() == 1) {
            self.contexts[new_context_ref].context_id = ids[0];
        } else if(ids.size() > 1) {
            assert(false && "unexpected error occurs when indexes");
        }

        return;
    }

    /// new header context.
    self.contexts[new_context_ref].context_id = new_context_id;
    self.max_context_id += 1;
}

void SymbolIndex::remove(SymbolIndex& other) {
    assert(other.contexts_table.size() == 1);
    auto& [path, _] = *other.contexts_table.begin();

    auto it = contexts_table.find(path);
    if(it != contexts_table.end()) {
        for(auto context_ref: it->second) {
            /// If this is the only ref of context id, remove the symbol id.
            auto context_id = contexts[context_ref].context_id;
            auto& ref_counts = header_context_id_ref_counts[context_ref];
            assert(ref_counts > 0);
            ref_counts -= 1;
            if(ref_counts == 0) {
                erased_context_ids.push_back(context_id);
            }

            /// Remove all refs of this context refs.
            for(auto& refs: independent_context_refs) {
                refs.erase(context_ref);
            }
            erased_context_refs.push_back(context_ref);
        }
    }
}

void SymbolIndex::update(SymbolIndex& index) {
    assert(index.contexts.size() == 1);
    remove(index);
    merge(index);
}

Symbol& SymbolIndex::getSymbol(std::int64_t symbol_id) {
    assert(contexts.size() == 1 && "please use merge for multiple contexts");
    if(auto it = symbols.find(symbol_id); it != symbols.end()) {
        return it->second;
    }

    /// If not found, create a new symbol and return it.
    auto symbol_ref = symbols.size();
    auto [it, _] = symbols.try_emplace(symbol_id, Symbol{.id = symbol_id});
    return it->second;
}

void SymbolIndex::addOccurrence(LocalSourceRange range,
                                std::int64_t target_symbol,
                                bool isDependent) {
    assert(contexts.size() == 1 && "please use merge for multiple contexts");
    auto& targets = occurrences[range];
    Occurrence occurrence;
    occurrence.set(isDependent);
    occurrence.target_symbol = target_symbol;
    assert(!targets.contains(occurrence));
    targets.insert(occurrence);
}

}  // namespace memory2

}  // namespace clice::index
