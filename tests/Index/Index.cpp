#include <gtest/gtest.h>
#include <Index/Index.h>

using namespace clice;

TEST(clice_test, index) {
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
}
