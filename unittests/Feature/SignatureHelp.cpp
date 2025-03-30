#include "Test/CTest.h"
#include "Feature/SignatureHelp.h"

namespace clice::testing {

namespace {

TEST(Feature, SignatureHelp) {
    const char* code = R"cpp(
void foo();

void foo(int x);

void foo(int x, int y);

int main() {
    foo(1, 2);
}
)cpp";

    CompilationParams params;
    params.content = code;
    params.srcPath = "main.cpp";
    params.command = "clang++ -std=c++20 main.cpp";
    params.completion = {"main.cpp", 9, 10};

    config::SignatureHelpOption options = {};
    auto result = feature::signatureHelp(params, options);
    /// EXPECT
    /// foo(int x, int y)
    /// foo(int x)
    /// foo()
}

}  // namespace

}  // namespace clice::testing

