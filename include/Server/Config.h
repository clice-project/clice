#pragma once

#include <string>
#include <vector>

#include "llvm/ADT/StringRef.h"

namespace clice::config {

/// read the config file, call when the program starts.
int parse(llvm::StringRef execute, llvm::StringRef filepath);

/// initialize the config, replace all predefined variables in the config file.
/// called in `Server::initialize`.
void init(std::string_view workplace);

struct ServerOption {
    std::string mode = "socket";
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

