#pragma once

#include <Clang/ParsedAST.h>

namespace clice {

class Scheduler {
    std::mutex mutex;
    llvm::StringMap<std::unique_ptr<ParsedAST>> parsedASTs;
};

}  // namespace clice
