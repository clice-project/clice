#pragma once

#include "SymbolIndex.h"
#include "FeatureIndex.h"
#include "Async/FileSystem.h"

namespace clice::index {

struct Index {
    std::optional<std::vector<char>> symbol;
    llvm::XXH128_hash_t symbolHash = {0, 0};

    std::optional<std::vector<char>> feature;
    llvm::XXH128_hash_t featureHash = {0, 0};

    static Shared<Index> build(ASTInfo& AST);

    async::Task<> write(std::string path) {
        if(symbol) {
            co_await async::fs::write(path + ".sidx", symbol->data(), symbol->size());
        }

        if(feature) {
            co_await async::fs::write(path + ".fidx", feature->data(), feature->size());
        }
    }
};

}  // namespace clice::index
