#include "../Test.h"
#include "Compiler/Compiler.h"

namespace clice {

TEST(Compiler, Directive) {

    const char* code = R"cpp(
#include <cstdio>

int main(){
    printf("Hello world");
    return 0;
}
)cpp";

    foreachFile("Compiler", [](std::string name, llvm::StringRef content) {
        llvm::SmallVector<const char*, 5> compileArgs = {
            "clang++",
            "-std=c++20",
            name.c_str(),
            "-resource-dir",
            "/home/ykiko/C++/clice2/build/lib/clang/20",
        };

        CompliationParams params;
        params.path = name.c_str();
        params.content = content;
        params.args = compileArgs;

        auto info = compile(params);
        ASSERT_TRUE(bool(info));

        for(auto& [id, file]: info->directives()) {
            for(auto& condition: file.conditions) {
                print("kind: {}, value: {}, loc: {}, range: {}\n",
                      support::enum_name(condition.kind),
                      support::enum_name(condition.value),
                      condition.loc.printToString(info->srcMgr()),
                      condition.conditionRange.printToString(info->srcMgr()));
            }

            for(auto& macro: file.macros) {
                print("name: {}, kind: {}, loc: {}, def: {}\n",
                      info->getTokenSpelling(macro.loc),
                      support::enum_name(macro.kind),
                      macro.loc.printToString(info->srcMgr()),
                      static_cast<const void*>(macro.macro));
            }
        }
    });
}

}  // namespace clice
