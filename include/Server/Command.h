#pragma once

#include <clang/Tooling/CompilationDatabase.h>

namespace clice {

class CompilationDatabase {

public:
    void load(clang::StringRef path);

    std::vector<const char*> lookup(clang::StringRef path);

private:
    std::unique_ptr<clang::tooling::CompilationDatabase> CDB;
};

}  // namespace clice
