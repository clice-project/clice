#include "Index/Contexts.h"
#include "Support/Ranges.h"

namespace clice::index {

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

std::uint32_t Contexts::alloc_cctx_id() {
    std::uint32_t new_cctx_id;
    if(erased_hctx_ids.empty()) {
        new_cctx_id = max_cctx_id;
        max_cctx_id += 1;
        cctx_hctx_refs.emplace_back(1);
        cctx_element_refs.emplace_back(0);
    } else {
        new_cctx_id = erased_cctx_ids.front();
        erased_hctx_ids.pop_front();
        cctx_hctx_refs[new_cctx_id] = 1;
        cctx_element_refs[new_cctx_id] = 0;
    }
    return new_cctx_id;
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

}  // namespace clice::index
