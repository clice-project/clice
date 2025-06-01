#pragma once

#include <deque>
#include <bitset>
#include <vector>
#include <variant>

#include "TUIndex.h"
#include "RawIndex.h"
#include "HeaderIndex.h"
#include "IncludeGraph.h"

namespace clice::index::memory {

struct Indices {
    IncludeGraph graph;
    llvm::DenseMap<clang::FileID, std::unique_ptr<RawIndex>> raw_indices;
};

Indices index(ASTInfo& AST);

}  // namespace clice::index::memory
