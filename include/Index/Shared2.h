#pragma once

#include "SymbolIndex.h"
#include "FeatureIndex.h"
#include "Async/FileSystem.h"

namespace clice::index {

struct Index2 {
    std::optional<SymbolIndex> symbol;
    llvm::XXH128_hash_t symbolHash = {0, 0};

    std::optional<FeatureIndex> feature;
    llvm::XXH128_hash_t featureHash = {0, 0};

    static Shared<Index2> build(ASTInfo& AST);

    async::Task<> write(std::string path) {
        if(symbol) {
            co_await async::fs::write(path + ".sidx", symbol->base, symbol->size);
        }

        if(feature) {
            co_await async::fs::write(path + ".fidx", feature->base, feature->size);
        }
    }
};

}  // namespace clice::index
