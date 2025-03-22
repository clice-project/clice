#include "Index/FeatureIndex.h"
#include "Support/Binary.h"
#include "Compiler/AST.h"

namespace clice::index {

namespace memory {

struct FeatureIndex {
    std::vector<feature::SemanticToken> tokens;
    std::vector<feature::FoldingRange> foldings;
    std::vector<feature::DocumentLink> links;
    feature::DocumentSymbols symbols;
};

}  // namespace memory

Shared<std::vector<char>> indexFeature(ASTInfo& info) {
    Shared<memory::FeatureIndex> indices;

    for(auto&& [fid, result]: feature::indexSemanticTokens(info)) {
        indices[fid].tokens = std::move(result);
    }

    for(auto&& [fid, result]: feature::indexFoldingRange(info)) {
        indices[fid].foldings = std::move(result);
    }

    for(auto&& [fid, result]: feature::indexDocumentLink(info)) {
        indices[fid].links = std::move(result);
    }

    for(auto&& [fid, result]: feature::indexDocumentSymbols(info)) {
        indices[fid].symbols = std::move(result);
    }

    Shared<std::vector<char>> result;

    for(auto&& [fid, index]: indices) {
        auto [buffer, size] = binary::serialize(static_cast<memory::FeatureIndex>(index));
        result.try_emplace(fid, std::move(buffer));
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

std::vector<feature::DocumentLink> FeatureIndex::documentLinks() const {
    auto array = binary::Proxy<memory::FeatureIndex>{base, base}.get<"links">();

    std::vector<feature::DocumentLink> result;
    result.reserve(array.size());

    /// FIXME: Use iterator or other thing to make cast easier.
    for(std::size_t i = 0, n = array.size(); i < n; ++i) {
        auto&& range = array[i];
        result.emplace_back(range.get<"range">().value(), range.get<"file">().as_string().str());
    }

    return result;
}

std::vector<feature::DocumentSymbol> FeatureIndex::documentSymbols() const {
    auto array = binary::Proxy<memory::FeatureIndex>{base, base}.get<"symbols">();

    std::vector<feature::DocumentSymbol> result;
    result.reserve(array.size());

    /// FIXME:

    return result;
}

}  // namespace clice::index
