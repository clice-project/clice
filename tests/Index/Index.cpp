#include <gtest/gtest.h>
#include <Index/SymbolSlab.h>
#include <Support/JSON.h>
#include <Compiler/Compiler.h>

using namespace clice;

TEST(clice, Index) {
    std::vector<const char*> compileArgs = {
        "clang++",
        "-std=c++20",
        "main.cpp",
        "-resource-dir",
        "/home/ykiko/C++/clice2/build/lib/clang/20",
    };
    const char* code = R"(
template<typename T, typename U> struct X {};

template<typename T> struct X<T, T> {};

void f() {
    X<char, int> y;
    X<int, int> x;
}
)";

    Compiler compiler("main.cpp", code, compileArgs);
    compiler.buildAST();
    SymbolSlab slab;
    auto csif = slab.index(compiler.context());
    auto value = json::serialize(csif);
    llvm::outs() << value << "\n";
}
