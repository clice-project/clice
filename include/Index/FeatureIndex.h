#pragma once

#include <vector>

#include "Shared.h"
#include "Feature/SemanticTokens.h"

#include "llvm/ADT/DenseMap.h"
#include "clang/Basic/SourceLocation.h"

namespace clice::index {

class FeatureIndex {
public:
    FeatureIndex(char* base, std::size_t size, bool own = true) :
        base(base), size(size), own(own) {}

    ~FeatureIndex() {
        if(own) {
            std::free(base);
        }
    }

    std::vector<feature::SemanticToken> semanticTokens();

private:
    char* base;
    std::size_t size;
    bool own;
};

Shared<FeatureIndex> indexFeature(ASTInfo& info);

}  // namespace clice::index
