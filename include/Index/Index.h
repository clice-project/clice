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
    std::unique_ptr<TUIndex> tu_index;
    llvm::DenseMap<clang::FileID, std::unique_ptr<RawIndex>> header_indices;
};

Indices index(CompilationUnit& unit);

}  // namespace clice::index::memory
