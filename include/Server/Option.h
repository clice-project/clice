#pragma once

#include <string>
#include <string_view>

namespace clice {

struct Option {
    std::string resource_dir;
    static Option parse(std::string_view execpath, std::string_view filepath);
};

extern Option option;

}  // namespace clice
