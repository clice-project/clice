#include "Compiler/CompilationUnit.h"
#include "Support/Binary.h"
#include "Index/FeatureIndex.h"

namespace clice::index {

namespace memory {

struct FeatureIndex {
    /// The path of source file.
    std::string path;

    /// The content of source file.
    std::string content;

    /// The index of semantic tokens.
    feature::SemanticTokens tokens;

    /// The index of folding ranges.
    feature::FoldingRanges foldings;

    /// The index of document links.
    feature::DocumentLinks links;

    /// The index of document symbols.
    feature::DocumentSymbols symbols;
};

}  // namespace memory

llvm::StringRef FeatureIndex::path() {
    binary::Proxy<memory::FeatureIndex> index{base, base};
    return index.get<"path">().as_string();
}

llvm::StringRef FeatureIndex::content() {
    binary::Proxy<memory::FeatureIndex> index{base, base};
    return index.get<"content">().as_string();
}

feature::SemanticTokens FeatureIndex::semanticTokens() const {
    binary::Proxy<memory::FeatureIndex> index{base, base};
    return binary::deserialize(index.get<"tokens">());
}

feature::FoldingRanges FeatureIndex::foldingRanges() const {
    binary::Proxy<memory::FeatureIndex> index{base, base};
    return binary::deserialize(index.get<"foldings">());
}

feature::DocumentLinks FeatureIndex::documentLinks() const {
    binary::Proxy<memory::FeatureIndex> index{base, base};
    return binary::deserialize(index.get<"links">());
}

feature::DocumentSymbols FeatureIndex::documentSymbols() const {
    binary::Proxy<memory::FeatureIndex> index{base, base};
    return binary::deserialize(index.get<"symbols">());
}

Shared<std::vector<char>> FeatureIndex::build(CompilationUnit& unit) {
    Shared<memory::FeatureIndex> indices;

    for(auto&& [fid, result]: feature::indexSemanticToken(unit)) {
        indices[fid].tokens = std::move(result);
    }

    for(auto&& [fid, result]: feature::indexFoldingRange(unit)) {
        indices[fid].foldings = std::move(result);
    }

    for(auto&& [fid, result]: feature::index_document_link(unit)) {
        indices[fid].links = std::move(result);
    }

    for(auto&& [fid, result]: feature::indexDocumentSymbol(unit)) {
        indices[fid].symbols = std::move(result);
    }

    Shared<std::vector<char>> result;

    for(auto&& [fid, index]: indices) {
        index.path = unit.file_path(fid);
        index.content = unit.file_content(fid);
        auto [buffer, _] = binary::serialize(index);
        result.try_emplace(fid, std::move(buffer));
    }

    return result;
}

}  // namespace clice::index
