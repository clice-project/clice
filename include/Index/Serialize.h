#include <Index/Indexer.h>
#include <Support/JSON.h>

namespace clice::index {

/// Serialize the index to JSON format.
json::Value toJson(const memory::Index& index);

/// This defines the structures that used to serialize the index to binary format.
/// All pointers are serialized as offsets to the beginning of the binary.
namespace binary {

template <typename T>
struct array {
    uint32_t offset;
    uint32_t length;
};

using string = array<char>;

using Symbol = index::Symbol<string, array>;

using Index = index::Index<string, array>;

}  // namespace binary

/// Serialize the index to binary format. The binary format is a sequence of bytes.
/// `binary::Index` is at the beginning of the binary.
std::vector<char> toBinary(const memory::Index& index);

/// TODO: ... support more serialization formats.

}  // namespace clice::index

