#pragma once

#include "Support/Support.h"

namespace clice::config {

/// Read the config file, call when the program starts.
void parse(llvm::StringRef execute, llvm::StringRef filepath);

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
    std::string index_directory = "${workplace}/.clice/index";
    std::string cache_directory = "${workplace}/.clice/cache";
    std::string resource_dictionary = "${binary}/../../lib/clang/${llvm_version}";
    std::vector<std::string> compile_commands_directorys = {"${workplace}/build"};
};

llvm::StringRef workplace();

const ServerOption& server();

const FrontendOption& frontend();

};  // namespace clice::config

