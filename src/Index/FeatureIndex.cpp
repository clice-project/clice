#include "Index/FeatureIndex.h"

namespace clice::index {

namespace {

class FeatureIndexBuilder {};

}  // namespace

Shared<FeatureIndex> indexFeature(ASTInfo& info) {
    return Shared<FeatureIndex>{};
}

}  // namespace clice::index
