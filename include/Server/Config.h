#pragma once

#include <vector>
#include <expected>

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/StringMap.h"

namespace clice::config {

/// Read the config file, call when the program starts.
std::expected<void, std::string> load(llvm::StringRef execute, llvm::StringRef filename);

/// Initialize the config, replace all predefined variables in the config file.
/// called in `Server::initialize`.
void init(std::string_view workplace);

struct ServerOptions {
    std::vector<std::string> compile_commands_dirs = {"${workspace}/build"};
    bool clang_tidy = false;
    size_t max_active_file = 8;
};

struct CacheOptions {
    std::string dir = "${workspace}/.clice/cache";
    uint32_t limit = 0;
};

struct IndexOptions {
    std::string dir = "${workspace}/.clice/index";
};

/// Configures what clang-tidy checks to run and options to use with them.
struct ClangTidyOptions {
    // A comma-separated list of globs specify which clang-tidy checks to run.
    std::string checks;
    llvm::StringMap<std::string> check_options;
};

struct Rule {
    std::string pattern;
    std::vector<std::string> append;
    std::vector<std::string> remove;
    std::string readonly;
    std::string header;
    std::vector<std::string> context;
};

extern llvm::StringRef version;
extern llvm::StringRef binary;
extern llvm::StringRef llvm_version;
extern llvm::StringRef workspace;

extern const ClangTidyOptions& clang_tidy;
extern const ServerOptions& server;
extern const CacheOptions& cache;
extern const IndexOptions& index;
extern llvm::ArrayRef<Rule> rules;

};  // namespace clice::config

