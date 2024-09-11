#include "toml++/toml.hpp"
#include "Server/Option.h"
#include <Support/FileSystem.h>

namespace clice {

// TODO: replace builtin variable in Setting
// e.g `{execute}` -> `execpath`

void Option::parse(std::string_view workplace) {
    llvm::SmallString<128> buffer;
    this->workplace = workplace;
    if(auto error = fs::real_path(argv[0], buffer)) {
        // FIXME: handle error
    }
    binary = path::parent_path(buffer);
    buffer.clear();
    path::append(buffer, path::parent_path(binary), "lib", "clang", clang_version);
    resource_dictionary = buffer.str();

    buffer.clear();
    path::append(buffer, workplace, "build");
    compile_commands_directory = buffer.str();
}

}  // namespace clice
