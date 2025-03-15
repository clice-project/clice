#include "Test/CTest.h"
#include "Feature/SignatureHelp.h"

namespace clice::testing {

namespace {

TEST(Feature, SignatureHelp) {
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
    params.file = "main.cpp";

    config::SignatureHelpOption options = {};
    auto result = feature::signatureHelp(params, options);
}

}  // namespace

}  // namespace clice::testing

