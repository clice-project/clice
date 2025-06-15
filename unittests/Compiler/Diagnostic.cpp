#include "Test/Test.h"
#include "Compiler/Diagnostic.h"
#include "Compiler/Compilation.h"

namespace clice::testing {

namespace {

using namespace clice;

TEST(Diagnostic, Error) {
    CompilationParams params;
    params.content = "";
    params.srcPath = "main.cpp";

    auto unit = compile(params);
    ASSERT_FALSE(unit);
    clice::println("{}", unit.error());
}

}  // namespace

}  // namespace clice::testing

