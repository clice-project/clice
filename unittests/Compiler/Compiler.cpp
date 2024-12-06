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

    CompliationParams params;
    params.path = "main.cpp";
    params.content = code;
    params.args = compileArgs;

    auto info = compile(params);
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

    CompliationParams params;
    params.path = "main.cpp";
    params.content = code;
    params.outpath = outpath;
    params.args = compileArgs;

    PCHInfo pch;
    ASSERT_TRUE(bool(clice::compile(params, pch)));

    params.addPCH(pch);

    auto ast = compile(params);
    ASSERT_TRUE(bool(ast));
}

TEST(Compiler, buildPCM) {
    const char* code = R"cpp(
export module A;

export int foo() {
    return 0;
}
)cpp";

    llvm::SmallString<128> outpath;
    if(auto error = llvm::sys::fs::createTemporaryFile("main", "pcm", outpath)) {
        llvm::errs() << error.message() << "\n";
        return;
    }

    if(auto error = fs::remove(outpath)) {
        llvm::errs() << error.message() << "\n";
        return;
    }

    llvm::SmallVector<const char*, 5> compileArgs = {
        "clang++",
        "-std=c++20",
        "main.cppm",
    };

    CompliationParams params;
    params.path = "main.cppm";
    params.content = code;
    params.outpath = outpath;
    params.args = compileArgs;

    PCMInfo pcm;
    ASSERT_TRUE(bool(clice::compile(params, pcm)));
    ASSERT_EQ(pcm.name, "A");

    const char* code2 = R"cpp(
import A;

int main(){
    foo();
    return 0;
}
)cpp";

    compileArgs = {
        "clang++",
        "-std=c++20",
        "main.cpp",
    };

    params.path = "main.cpp";
    params.content = code2;
    params.args = compileArgs;
    params.addPCM(pcm);

    auto info = compile(params);
    ASSERT_TRUE(bool(info));
}

TEST(Compiler, codeCompleteAt) {
    const char* code = R"cpp(
export module A;
export int foo = 1;
)cpp";

    llvm::SmallVector<const char*, 5> compileArgs = {
        "clang++",
        "-std=c++20",
        "main.cppm",
    };

    CompliationParams params;
    params.path = "main.cppm";
    params.content = code;
    params.args = compileArgs;
    params.line = 3;
    params.column = 10;

    auto consumer = new clang::PrintingCodeCompleteConsumer({}, llvm::outs());
    auto info = compile(params, consumer);
    ASSERT_TRUE(bool(info));

    /// TODO: add tests in the case of PCH, PCM and override file.
}

}  // namespace
