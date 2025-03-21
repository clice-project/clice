#include "Index/Shared2.h"
#include "Compiler/Compilation.h"

namespace clice::index {

Shared<Index2> Index2::build(ASTInfo& AST) {
    llvm::DenseMap<clang::FileID, Index2> indices;

    auto symbolIndices = index::index(AST);
    for(auto& [fid, index]: symbolIndices) {
        indices[fid].symbol.emplace(std::move(index));
    }

    auto featureIndices = index::indexFeature(AST);
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
