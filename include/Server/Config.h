#pragma once

#include <vector>
#include <expected>

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringRef.h"

namespace clice::config {

extern llvm::StringRef version;
extern llvm::StringRef binary;
extern llvm::StringRef llvm_version;
extern llvm::StringRef workspace;

struct ProjectOptions {
    bool root = true;

    bool clang_tidy = false;

    std::size_t max_active_file = 8;

    std::string cache_dir = "${workspace}/.clice/cache";

    std::string index_dir = "${workspace}/.clice/index";

    std::string logging_dir = "${workspace}/.clice/logging";

    std::vector<std::string> compile_commands_dirs = {"${workspace}/build"};
};

struct Rule {
    std::string pattern;
    std::vector<std::string> append;
    std::vector<std::string> remove;
    std::string readonly;
    std::string header;
    std::vector<std::string> context;
};

struct Config {
    std::string workspace;

    ProjectOptions project;

    llvm::SmallVector<Rule> rules;

    auto parse(llvm::StringRef workspace) -> std::expected<void, std::string>;
};

};  // namespace clice::config
