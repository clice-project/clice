#include "../Test.h"
#include <Compiler/Compiler.h>
#include <Feature/CodeCompletion.h>

namespace {

using namespace clice;

TEST(CodeCompletion, non_self_contain) {
    llvm::SmallString<64> main, header;
    path::append(main, test_dir(), "CodeCompletion", "non-self-contain.cpp");
    path::append(header, test_dir(), "CodeCompletion", "non-self-contain.h");

    std::vector<const char*> compileArgs = {
        "clang++",
        "-std=c++20",
        main.c_str(),
        "-resource-dir",
        "/home/ykiko/C++/clice2/build/lib/clang/20",
    };

    Compiler compiler(compileArgs);
    proto::Position position;
    position.line = 2;
    position.character = 6;
    auto completionItems = feature::codeCompletion(compiler, header, position, {});
}

}  // namespace
