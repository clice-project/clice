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
    params.srcPath = "main.cpp";
    params.content = code;
    params.args = compileArgs;

    auto info = compile(params);
    ASSERT_TRUE(bool(info));
}

TEST(Compiler, ComputeBounds) {

    llvm::SmallVector<const char*, 5> compileArgs = {
        "clang++",
        "-std=c++20",
        "main.cpp",
        "-resource-dir",
        "/home/ykiko/C++/clice2/build/lib/clang/20",
    };

    const char* code = R"cpp(
#include <cstdio>
int main(){
    printf("Hello world");
    return 0;
})cpp";

    /// Test in no header file.
    CompliationParams params;
    params.srcPath = "main.cpp";
    params.args = compileArgs;
    params.content = code;
    params.computeBounds();

    ASSERT_TRUE(params.bounds.has_value());
    ASSERT_EQ(params.bounds->Size, 19);

    params.bounds.reset();

    compileArgs = {"clang++", "-std=c++20", "main.cpp"};

    std::unique_ptr<vfs::InMemoryFileSystem> vfs(new vfs::InMemoryFileSystem);
    const char* header = R"cpp(
#include "target.h"
)cpp";

    vfs->addFile("header.h", 0, llvm::MemoryBuffer::getMemBuffer(header));
    vfs->addFile("header2.h", 0, llvm::MemoryBuffer::getMemBuffer(""));
    vfs->addFile("target.h", 0, llvm::MemoryBuffer::getMemBuffer(""));

    code = R"cpp(
#include "header2.h"
#include "header.h"
int main(){
    return 0;
})cpp";

    compileArgs = {
        "clang++",
        "-std=c++20",
        "main.cpp",
    };

    params.srcPath = "main.cpp";
    params.args = compileArgs;
    params.content = code;
    params.vfs = std::move(vfs);
    params.computeBounds("target.h");

    ASSERT_TRUE(params.bounds.has_value());
    ASSERT_EQ(params.bounds->Size, 43);
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
    params.content = code;
    params.srcPath = "main.cpp";
    params.outPath = outpath;
    params.args = compileArgs;
    params.computeBounds();

    PCHInfo pch;
    ASSERT_TRUE(bool(clice::compile(params, pch)));

    params.bounds.reset();
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
    params.srcPath = "main.cppm";
    params.content = code;
    params.outPath = outpath;
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

    params.srcPath = "main.cpp";
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
    params.srcPath = "main.cppm";
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
