#include "Index/Contexts.h"
#include "Support/Ranges.h"

namespace clice::index {

#ifdef NDEBUG
#define BREAK_WHEN_RELEASE
#else
#define BREAK_WHEN_RELEASE break
#endif

std::uint32_t Contexts::alloc_hctx_id() {
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

void Contexts::merge_one(this Contexts& self,
                         Contexts& other,
                         std::vector<InsertSlot> dependent_elements,
                         std::vector<InsertSlot> independent_elements) {
    assert(other.max_cctx_id == 1 && other.erased_cctx_ids.empty() &&
           "other must have only one canonical context");
    Bitmap flag = self.erased_flag();

    for(auto& [_, new_elem_id]: dependent_elements) {
        /// If new elem id equals to -1, the canonical context must introduce
        /// a new canonical context in self.
        if(new_elem_id == -1) {
            flag.reset();
            break;
        }

        flag &= other.dependent_element_states[new_elem_id];
    }

    std::uint32_t new_cctx_id = -1;

    if(flag.none()) {
        /// If flag is none, this is a new canonical context.
        if(!self.erased_cctx_ids.empty()) {
            new_cctx_id = self.erased_cctx_ids.front();
            self.erased_cctx_ids.pop_front();
            self.cctx_element_refs[new_cctx_id] = 1;
            self.cctx_element_refs[new_cctx_id] = other.cctx_element_refs.front();
        } else {
            new_cctx_id = self.max_cctx_id;
            self.max_cctx_id += 1;
            self.cctx_hctx_refs.emplace_back(1);
            self.cctx_element_refs.emplace_back(other.cctx_element_refs.front());
        }
    } else {
        bool found = false;
        for(auto self_cctx_id = 0; self_cctx_id < flag.size(); self_cctx_id++) {
            if(flag.test(self_cctx_id) &&
               self.cctx_element_refs[self_cctx_id] == other.cctx_element_refs.front()) {
                new_cctx_id = self_cctx_id;
                assert(!found && "already found");
                found = true;
                BREAK_WHEN_RELEASE;
            }
        }
        assert(found && "we must found one");
    }

    /// A map between old header context id to new header context id.
    llvm::SmallDenseMap<std::uint32_t, std::uint32_t> hctx_map;

    /// Insert new header context.
    for(auto& [path, contexts]: other.header_contexts) {
        auto it = self.header_contexts.find(path);
        assert(it == self.header_contexts.end() && "remove before merging");
        for(auto& context: contexts) {
            context.cctx_id = new_cctx_id;
            auto new_hcxt_id = self.alloc_hctx_id();
            hctx_map.try_emplace(context.hctx_id, new_hcxt_id);
            context.hctx_id = new_hcxt_id;
        }
        self.header_contexts.try_emplace(path, std::move(contexts));
    }

    /// Update all element states.
}

void Contexts::merge(this Contexts& self,
                     Contexts& other,
                     std::vector<InsertSlot> dependent_elements,
                     std::vector<InsertSlot> independent_elements) {
    Bitmap default_flag = self.erased_flag();
    llvm::SmallVector<Bitmap> flags;

    /// Note that the flags may contains some erased slot,
    /// we will filter them in the end, so don't worry here.
    for(auto i = 0; i < other.max_cctx_id; i++) {
        flags.emplace_back(default_flag);
    }

    /// Calculate the new element states.
    for(auto& [old_elem_id, new_elem_id]: dependent_elements) {
        auto& old_state = other.dependent_element_states[old_elem_id];

        if(new_elem_id == -1) {}

        for(auto i = 0; i < other.max_cctx_id; i++) {
            if(other.dependent_element_states[old_elem_id].test(i)) {
                flags[i] &= self.dependent_element_states[new_elem_id];
            }
        }
    }

    /// A map between old cctx id and new cctx id.
    llvm::SmallDenseMap<std::uint32_t, std::uint32_t> cctx_map;

    auto erased_cctx_ids = llvm::SmallDenseSet<std::uint32_t>(other.erased_cctx_ids.begin(),
                                                              other.erased_cctx_ids.end());

    /// Finally we need to make new context.
    for(auto [other_cctx_id, flag]: views::enumerate(flags)) {
        /// Skip erased canonical context id.
        if(erased_cctx_ids.contains(other_cctx_id)) {
            continue;
        }

        std::uint32_t new_cctx_id = -1;
        if(flag.none()) {
            /// If the flag is none, represents that we need to allocs
            /// a new canonical context for it.
            if(!self.erased_cctx_ids.empty()) {
                new_cctx_id = self.erased_cctx_ids.front();
                self.erased_cctx_ids.pop_front();
                self.cctx_element_refs[new_cctx_id] = 1;
                self.cctx_element_refs[new_cctx_id] = other.cctx_element_refs[other_cctx_id];
            } else {
                new_cctx_id = self.max_cctx_id;
                self.max_cctx_id += 1;
                self.cctx_hctx_refs.emplace_back(1);
                self.cctx_element_refs.emplace_back(other.cctx_element_refs[other_cctx_id]);
            }
        } else {
            bool found = false;
            /// Otherwise, it represents that at least we have one canonical
            /// could be reused.
            for(auto self_cctx_id: views::iota(flag.size())) {
                if(!flag.test(self_cctx_id)) {
                    continue;
                }

                if(self.cctx_element_refs[self_cctx_id] == other.cctx_element_refs[other_cctx_id]) {
                    new_cctx_id = self_cctx_id;
                    assert(!found && "already found");
                    found = true;

#ifndef NDEBUG
                    /// Only break in debug mode for testing.
                    break;
#endif
                }
            }

            /// We must found the only one.
            if(!found) {
                std::abort();
            }
        }

        cctx_map.try_emplace(other_cctx_id, new_cctx_id);
    }

    /// Okay, now we could updates all headers from ...
    for(auto& [path, contexts]: other.header_contexts) {
        /// 一个问题是，other 的头文件上下文我们已经插入过了吗?
        /// 如果是的，那只需要 cctx id 就行了
        /// 如果不是呢？那就挨个插入到 self 里面 ...
        /// 有一个问题是关于 header context id 的分配问题 ...
    }
}

void Contexts::remove(this Contexts& self, llvm::StringRef path) {
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

    /// Remove all refs to this header context id.
    for(auto& state: self.independent_element_states) {
        for(auto hctx_id: erased_hctx_ids) {
            state.erase(hctx_id);
        }
    }

    /// Remove all refs to this canonical context id.
    Bitmap erased_flag = self.erased_flag();

    for(auto& state: self.dependent_element_states) {
        state &= erased_flag;
    }
}

}  // namespace clice::index
