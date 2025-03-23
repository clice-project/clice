#include "Index/Index.h"
#include "Compiler/AST.h"

namespace clice::index {

Shared<Index> Index::build(ASTInfo& AST) {
    llvm::DenseMap<clang::FileID, Index> indices;

    auto symbolIndices = SymbolIndex::build(AST);
    for(auto& [fid, index]: symbolIndices) {
        indices[fid].symbol.emplace(std::move(index));
    }

    auto featureIndices = FeatureIndex::build(AST);
    for(auto& [fid, index]: featureIndices) {
        indices[fid].feature.emplace(std::move(index));
    }

    for(auto& [fid, index]: indices) {
        if(index.symbol) {
            auto data = llvm::ArrayRef<uint8_t>(reinterpret_cast<uint8_t*>(index.symbol->data()),
                                                index.symbol->size());
            index.symbolHash = llvm::xxh3_128bits(data);
        }

        if(index.feature) {
            auto data = llvm::ArrayRef<uint8_t>(reinterpret_cast<uint8_t*>(index.feature->data()),
                                                index.feature->size());
            index.featureHash = llvm::xxh3_128bits(data);
        }
    }

    return indices;
}

}  // namespace clice::index
