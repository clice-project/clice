#pragma once

#include "Async/Async.h"
#include "Compiler/Command.h"

namespace clice {

class ASTInfo;



class Scheduler {
public:
    async::Task<ASTInfo> build(llvm::StringRef tu);

private:
    CompilationDatabase database;
};

}  // namespace clice
