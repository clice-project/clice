#include <src/Compiler/Selection.cpp>
#include <gtest/gtest.h>

#include "../Test.h"

namespace clice {

namespace {

TEST(Selection, Selection) {
    const char* code = R"cpp(

struct X {};

)cpp";

    CompilationParams params;
    params.content = code;
    params.srcPath = "main.cpp";
    params.command = "clang++ -std=c++20 main.cpp";

    auto info = compile(params);
    // ASSERT_TRUE(bool(info));

    SelectionBuilder builder(0, 0, info->context(), info->tokBuf());
}

}  // namespace

}  // namespace clice
