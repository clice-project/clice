#pragma once

#include "Memory.h"

namespace clice::index {

namespace binary {

template <typename T>
struct Array {
    /// Offset to base pointer.
    std::uint32_t offset = 0;

    /// Size of the array.
    std::uint32_t size = 0;
};

using String = Array<char>;

#define MAKE_CLANG_HAPPY
#include "Index.inl"

}  // namespace binary

/// Convert `memory::Index` to `binary::Index`.
binary::Index toBinary(const memory::Index& index);

}  // namespace clice::index
