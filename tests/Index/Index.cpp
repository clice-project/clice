#include <gtest/gtest.h>
#include <Index/SymbolSlab.h>
#include <Support/JSON.h>
#include <Compiler/Compiler.h>
#include <Support/FileSystem.h>

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
template<typename T, typename U> struct X {
    using type = char;
};

template<typename T> struct X<T, T> {
    using type = int;
};

void f() {
    typename X<char, int>::type y;
    typename X<int, int>::type x;
}
)";

    Compiler compiler("main.cpp", code, compileArgs);
    compiler.buildAST();
    SymbolSlab slab;
    auto csif = slab.index(compiler.context());
    auto value = json::serialize(csif);
    std::error_code EC;
    llvm::raw_fd_ostream fileStream("output.json", EC);
    fileStream << value << "\n";
    llvm::outs() << value << "\n";
}
