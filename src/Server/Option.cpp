#include "toml++/toml.hpp"
#include "Server/Option.h"

namespace clice {

// TODO: replace builtin variable in Setting
// e.g `{execute}` -> `execpath`

Option Option::parse(std::string_view execpath, std::string_view filepath) {
    auto config = toml::parse_file(filepath);
    auto clang = config["clang"];
    auto resource_dir = clang["resource_dir"].as_string()->get();
    return Option{resource_dir};
}

}  // namespace clice
