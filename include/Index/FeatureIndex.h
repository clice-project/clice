#pragma once

#include <vector>

#include "llvm/ADT/DenseMap.h"
#include "clang/Basic/SourceLocation.h"

namespace clice::index {

template <typename T>
using SharedIndex = llvm::DenseMap<clang::FileID, T>;

}
