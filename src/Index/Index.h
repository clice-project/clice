#include "Index/SymbolIndex.h"

namespace clice::index {

namespace memory {

template <typename T>
using Array = std::vector<T>;

using String = std::string;

#define MAKE_CLANGD_HAPPY
#include "Index.inl"

}  // namespace memory

/// This namespace defines the binary format of the index file. Generally,
/// transform all pointer to offset to base address and cache location in the
/// location array. And data only will be deserialized when it is accessed.
namespace binary {

template <typename T>
struct Array {
    /// Offset to index start.
    uint32_t offset;

    /// Number of elements.
    uint32_t size;
};

using String = Array<char>;

#define MAKE_CLANGD_HAPPY
#include "Index.inl"

}  // namespace binary

SymbolIndex serialize(const memory::SymbolIndex& index);

binary::FeatureIndex* serialize(const memory::FeatureIndex& index);

}  // namespace clice::index
