#include "../Test.h"
#include "Compiler/Compiler.h"
#include "Support/Support.h"

#include "Feature/CodeCompletion.h"

namespace {

using namespace clice;

TEST(Feature, CodeCompletion) {
    const char* code = R"cpp(
int foo = 2;

int main() {
    foo = 2;
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

    auto result = feature::codeCompletion(params, 5, 7, "main.cpp", {});
    for(auto& item: result) {
        llvm::outs() << std::format("kind: {}, label: {}, range: {}\n",
                                    item.kind.name(),
                                    item.label,
                                    json::serialize(item.textEdit.range))
                     << "\n";
    }
}

}  // namespace
