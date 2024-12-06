#pragma once

#include "Index.h"

namespace clice::index {

namespace memory {

using String = std::string;

template <typename T>
using Array = std::vector<T>;

#define MAKE_CLANG_HAPPY
#include "Index.inl"

}  // namespace memory

/// Index the AST information.
memory::Index index(ASTInfo& info);

/// Convert `memory::Index` to JSON.
json::Value toJSON(const memory::Index& index);

}  // namespace clice::index
