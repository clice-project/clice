#include "../Test.h"
#include <AST/Selection.h>
#include <Compiler/Compiler.h>

namespace {

using namespace clice;

TEST(clice, CodeCompletion) {
    foreachFile("CodeCompletion", [](std::string file, llvm::StringRef content) {
        if(file.ends_with("test.cpp")) {
            std::vector<const char*> compileArgs = {
                "clang++",
                "-std=c++20",
                file.c_str(),
                "-resource-dir",
                "/home/ykiko/C++/clice2/build/lib/clang/20",
            };
            auto compiler = Compiler(compileArgs);
            compiler.codeCompletion(file, 10, 9);
        }
    });
}

}  // namespace

