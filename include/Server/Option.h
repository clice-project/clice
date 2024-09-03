#pragma once

#include <string>

namespace clice {

struct Option {
    std::string compile_commands_directory;
    std::string resource_dictionary;

    void parse(int argc, const char** argv);
};

namespace global {
extern struct Option option;
};

}  // namespace clice
