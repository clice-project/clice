#include "../Test.h"
#include "Server/Indexer.h"
#include "Compiler/Compiler.h"

namespace clice {

namespace {

TEST(Server, Indexer) {
    const char* test = R"cpp(
#ifndef TEST_H
struct X {};
#else
struct Y {};
#endif
)cpp";

    const char* src1 = R"cpp(
#include "test.h"
)cpp";

    const char* src2 = R"cpp(
#define TEST_H
#include "test.h"
)cpp";

    CompilationParams params;
    params.remappedFiles.emplace_back("./test.h", test);
    params.content = src1;
    params.srcPath = "src1.cpp";
    params.command = "clang++ -std=c++20 src1.cpp";
    auto info = compile(params);
    assert(bool(info));

    params.content = src2;
    params.srcPath = "src2.cpp";
    params.command = "clang++ -std=c++20 src2.cpp";
    auto info2 = compile(params);
    assert(bool(info2));

    Indexer indexer;
    indexer.index("src1.cpp", *info);
    indexer.index("src2.cpp", *info2);
    indexer.save();
}

}  // namespace

}  // namespace clice
