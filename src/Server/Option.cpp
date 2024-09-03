#include "toml++/toml.hpp"
#include "Server/Option.h"
#include <Support/FileSystem.h>

namespace clice {

// TODO: replace builtin variable in Setting
// e.g `{execute}` -> `execpath`

void Option::parse(int argc, const char** argv) {
    auto execute = argv[0];
    // compile_commands_directory = path::parent_path(execute);
}

namespace global {
Option option;
};

}  // namespace clice
