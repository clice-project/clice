#include "../Test.h"
#include "Compiler/Compiler.h"
#include <Support/FileSystem.h>

namespace {

using namespace clice;

TEST(Compiler, buildAST) {
    const char* code = R"cpp(
#include <cstdio>

int main(){
    printf("Hello world");
    return 0;
}
)cpp";

    llvm::SmallVector<const char*, 5> compileArgs = {
        "clang++",
        "-std=c++20",
        "main.cpp",
        "-resource-dir",
        "/home/ykiko/C++/clice2/build/lib/clang/20",
    };

    auto info = buildAST("main.cpp", code, compileArgs);
    ASSERT_TRUE(bool(info));
}

TEST(Compiler, buildPCH) {
    const char* code = R"cpp(
#include <cstdio>

int main(){
    printf("Hello world");
    return 0;
}
)cpp";

    llvm::SmallVector<const char*, 5> compileArgs = {
        "clang++",
        "-std=c++20",
        "main.cpp",
        "-resource-dir",
        "/home/ykiko/C++/clice2/build/lib/clang/20",
    };

    llvm::SmallString<128> outpath;
    if(auto error = llvm::sys::fs::createTemporaryFile("main", "pch", outpath)) {
        llvm::errs() << error.message() << "\n";
        return;
    }

    if(auto error = fs::remove(outpath)) {
        llvm::errs() << error.message() << "\n";
        return;
    }

    auto pch = clice::buildPCH("main.cpp", code, outpath, compileArgs);
    ASSERT_TRUE(bool(pch));

    Preamble preamble;
    preamble.addPCH(*pch);

    auto ast = buildAST("main.cpp", code, compileArgs, &preamble);
    ASSERT_TRUE(bool(ast));
}

}  // namespace
