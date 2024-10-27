#pragma once

#include <string>
#include <vector>

namespace clice::config {

/// read the config file, call when the program starts.
int parse(int argc, const char** argv);

/// initialize the config, replace all predefined variables in the config file.
/// called in `Server::initialize`.
void init(std::string_view workplace);

struct ServerOption {
    std::string mode;
    unsigned int port;
    std::string address;
};

struct FrontendOption {
    std::vector<std::string> append;
    std::vector<std::string> remove;
    std::string resource_dictionary = "${binary}/../lib/clang/${llvm_version}";
    std::string compile_commands_directory = "${workplace}/build";
};

const ServerOption& server();

const FrontendOption& frontend();

};  // namespace clice::config

