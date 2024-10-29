#include <Index/Serialize.h>

namespace clice::index {

json::Value toJson(const memory::Index& index) {
    return json::serialize(index);
}

std::vector<char> toBinary(const memory::Index& index) {
    // TODO:
    return {};
}

}  // namespace clice::index
