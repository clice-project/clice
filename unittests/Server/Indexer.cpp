#include "../Test.h"
#include "Server/Indexer.h"
#include "Compiler/Compiler.h"

namespace clice {

namespace {

TEST(Server, Indexer) {
    const char* code = R"cpp(
#include <iostream>
)cpp";
    CompilationParams params;
    params.content = code;
    params.srcPath = "main.cpp";
    params.command = "clang++ -std=c++20 main.cpp";
    auto info = compile(params);
    assert(bool(info));

    Indexer indexer;
}

}  // namespace

}  // namespace clice
