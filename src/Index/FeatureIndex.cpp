#include "Index/FeatureIndex.h"
#include "Support/Binary.h"

namespace clice::index {

namespace memory {

struct FeatureIndex {
    std::vector<feature::SemanticToken> tokens;
};

}  // namespace memory

Shared<FeatureIndex> indexFeature(ASTInfo& info) {
    Shared<memory::FeatureIndex> indices;

    for(auto&& [fid, result]: feature::semanticTokens(info)) {
        indices[fid].tokens = std::move(result);
    }

    Shared<FeatureIndex> result;

    for(auto&& [fid, index]: indices) {
        auto [buffer, size] = binary::binarify(static_cast<memory::FeatureIndex>(index));
        result.try_emplace(fid, FeatureIndex{const_cast<void*>(buffer.base), size, true});
    }

    return result;
}

llvm::ArrayRef<feature::SemanticToken> FeatureIndex::semanticTokens() const{
    return binary::Proxy<memory::FeatureIndex>{base, base}.get<"tokens">().as_array();
}

}  // namespace clice::index
