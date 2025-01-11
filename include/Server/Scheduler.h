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
class FileController {
public:
    FileController(CompilationDatabase& database, llvm::ArrayRef<Rule> rules) :
        database(database), rules(rules) {}

    async2::Task<> open(llvm::StringRef path);

    async2::Task<> update(llvm::StringRef path);

    async2::Task<> close(llvm::StringRef path);

private:
    CompilationDatabase& database;
    llvm::ArrayRef<Rule> rules;
};

}  // namespace clice
