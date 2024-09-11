#pragma once

#include <string>

namespace clice {

struct Option {
    /// predefined variables.
    struct {
        int argc;
        const char** argv;
        /// clice binary dictionary.
        std::string binary;
        /// workplace dictionary.
        std::string workplace;
    };

    std::string resource_dictionary;
    std::string compile_commands_directory;

    void parse(std::string_view workplace);
};

}  // namespace clice
