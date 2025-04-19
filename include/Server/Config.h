#pragma once

#include <vector>
#include <expected>

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringRef.h"

namespace clice::config {

/// Read the config file, call when the program starts.
std::expected<void, std::string> load(llvm::StringRef execute, llvm::StringRef filename);

/// Initialize the config, replace all predefined variables in the config file.
/// called in `Server::initialize`.
void init(std::string_view workplace);

struct ServerOptions {
    /// The dictionary of all compile commands.
    std::vector<std::string> compile_commands_dirs = {
        "${workspace}/build",
    };
};

struct CacheOptions {
    /// Directory for storing index files.
    std::string dir = "${workspace}/.clice/index";
    uint32_t limit = 0;
};

struct IndexOptions {
    /// Directory for storing PCH and PCM.
    std::string dir = "${workspace}/.clice/cache";

    bool implicitInstantiation = true;
};

struct Rule {
    /// The glob pattern used to match file.
    std::vector<std::string> patterns;

    /// The commands to append to the original command list.
    std::vector<std::string> append;

    /// The commands to remove from the original command list.
    std::vector<std::string> remove;

    /// Whether this file is readonly.
    std::string readonly;

    /// Whether this file is treated as header.
    std::string header;

    /// The explicit header context of this file.
    std::vector<std::string> context;
};

/// The server version.
extern llvm::StringRef version;

extern llvm::StringRef binary;

extern llvm::StringRef llvm_version;

extern llvm::StringRef workspace;

/// The server option instance.
extern const ServerOptions& server;

/// The cache option instance.
extern const CacheOptions& cache;

/// The index option instance.
extern const IndexOptions& index;

/// The rule instances.
extern llvm::ArrayRef<Rule> rules;

};  // namespace clice::config

