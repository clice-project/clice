#include "Test/Test.h"
#include <Compiler/Compiler.h>
#include <Compiler/Diagnostic.h>

namespace {

using namespace clice;

TEST(clice, Diagnostic) {
    // foreachFile("Diagnostic", [](std::string file, llvm::StringRef content) {
    //     std::vector<const char*> compileArgs = {
    //         "clang++",
    //         "-std=c++20",
    //         file.c_str(),
    //         "-resource-dir",
    //         "/home/ykiko/C++/clice2/build/lib/clang/20",
    //     };
    //     Compiler compiler(file, content, compileArgs, new DiagnosticCollector());
    //     compiler.buildAST();
    // });
}

}  // namespace
