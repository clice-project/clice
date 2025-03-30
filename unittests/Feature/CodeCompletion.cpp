#include "Test/CTest.h"
#include "Feature/CodeCompletion.h"

namespace clice::testing {

namespace {

TEST(Feature, CodeCompletion) {
    const char* code = R"cpp(
int foo = 2;

int main() {
    foo = 2;
}
)cpp";

    CompilationParams params;
    params.content = code;
    params.srcPath = "main.cpp";
    params.command = "clang++ -std=c++20 main.cpp";
    params.completion = {"main.cpp", 5, 6};

    config::CodeCompletionOption options = {};
    auto result = feature::codeCompletion(params, options);
}

}  // namespace

}  // namespace clice::testing

