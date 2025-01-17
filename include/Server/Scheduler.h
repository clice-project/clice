#pragma once

#include "Cache.h"

namespace clice {

struct Rule {
    /// The file name pattern.
    std::string pattern;

    /// ...
    std::vector<std::string> append;

    /// ...
    std::vector<std::string> remove;

    std::string readonly;

    std::string header;

    std::vector<std::string> context;
};

/// This class is responsible for managing all opened files.
class Scheduler {
public:
    Scheduler(CompilationDatabase& database, llvm::ArrayRef<Rule> rules) :
        database(database), rules(rules) {}

    async::Task<> open(llvm::StringRef path);

    async::Task<> update(llvm::StringRef path);

    async::Task<> close(llvm::StringRef path);

private:
    CompilationDatabase& database;

    llvm::ArrayRef<Rule> rules;

    struct File {};

    llvm::StringMap<File> files;
};

}  // namespace clice
