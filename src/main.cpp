#include <clang/AST/Decl.h>
#include <cassert>
#include <AST/Diagnostic.h>
#include <AST/ParsedAST.h>
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/TargetSelect.h"

const char* source = R"(
#include <vector>
int main(){
    int x;
    return 0;
}
)";

int main(int argc, const char** argv) {
    // clice::execute_path = argv[0];
    auto args = std::vector<const char*>{
        "/usr/bin/c++",
        "main.cpp",
        "-resource-dir",
        "/home/ykiko/C++/clice2/build/lib/clang/20",
        "-Wall",
    };
    auto preamble = clice::Preamble::build("main.cpp", source, args);
    auto ast = clice::ParsedAST::build("main.cpp", source, args, preamble.get());
    ast->tu->dump();
}
