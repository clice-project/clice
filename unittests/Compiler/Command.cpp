#include "../Test.h"
#include "Compiler/Command.h"

namespace {

TEST(clice, CommandManager) {
    clice::CommandManager manager("/home/ykiko/C++/clice2/tests");
    auto args = manager.lookup("another_file.cpp");
    for(auto arg: args) {
        llvm::outs() << arg << " ";
    }
    llvm::outs() << "\n";
}

}  // namespace
