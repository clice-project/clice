#include "Index/FeatureIndex.h"
#include "Support/Binary.h"
#include "Compiler/AST.h"

namespace clice::index {

namespace memory {

struct FeatureIndex {
    std::vector<feature::SemanticToken> tokens;
    std::vector<feature::FoldingRange> foldings;
};

}  // namespace memory

Shared<FeatureIndex> indexFeature(ASTInfo& info) {
    Shared<memory::FeatureIndex> indices;

    for(auto&& [fid, result]: feature::indexSemanticTokens(info)) {
        indices[fid].tokens = std::move(result);
    }

    for(auto&& [fid, result]: feature::indexFoldingRange(info)) {
        indices[fid].foldings = std::move(result);
    }

    Shared<FeatureIndex> result;

    for(auto&& [fid, index]: indices) {
        /// FIXME: Figure out why index result will contain them.
        auto& SM = info.srcMgr();
        auto loc = SM.getLocForStartOfFile(fid);
        if(SM.isWrittenInBuiltinFile(loc) || SM.isWrittenInCommandLineFile(loc) ||
           SM.isWrittenInScratchSpace(loc)) {
            continue;
        }

        auto [buffer, size] = binary::binarify(static_cast<memory::FeatureIndex>(index));
        result.try_emplace(
            fid,
            FeatureIndex{static_cast<char*>(const_cast<void*>(buffer.base)), size, true});
    }

    return result;
}

std::vector<feature::SemanticToken> FeatureIndex::semanticTokens() const {
    return binary::Proxy<memory::FeatureIndex>{base, base}.get<"tokens">().as_array();
}

std::vector<feature::FoldingRange> FeatureIndex::foldingRanges() const {
    auto array = binary::Proxy<memory::FeatureIndex>{base, base}.get<"foldings">();

    std::vector<feature::FoldingRange> result;
    result.reserve(array.size());

    /// FIXME: Use iterator or other thing to make cast easier.
    for(std::size_t i = 0, n = array.size(); i < n; ++i) {
        auto&& range = array[i];
        result.emplace_back(range.get<"range">().value(),
                            range.get<"kind">().value(),
                            range.get<"text">().as_string().str());
    }

    return result;
}

}  // namespace clice::index
