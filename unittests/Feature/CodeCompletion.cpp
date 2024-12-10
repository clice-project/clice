#include "../Test.h"
#include "Feature/CodeCompletion.h"

namespace clice {

namespace {

TEST(Feature, CodeCompletion) {
    const char* code = R"cpp(
int foo = 2;

int main() {
    foo = 2;
}
)cpp";

    CompliationParams params;
    params.content = code;
    params.srcPath = "main.cpp";
    params.command = "clang++ -std=c++20 main.cpp";
    params.file = "main.cpp";

    auto result = feature::codeCompletion(params, 5, 7, "main.cpp", {});
    // for(auto& item: result) {
    //     llvm::outs() << std::format("kind: {}, label: {}, range: {}\n",
    //                                 item.kind.name(),
    //                                 item.label,
    //                                 json::serialize(item.textEdit.range))
    //                  << "\n";
    // }
}

}  // namespace

}  // namespace clice

