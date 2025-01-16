#include "Test/CTest.h"
#include "Feature/InlayHint.h"
#include "Basic/SourceConverter.h"

namespace clice::testing {

namespace {

const config::InlayHintOption LikeClangd{
    .maxLength = 20,
    .maxArrayElements = 10,
    .structSizeAndAlign = false,
    .memberSizeAndOffset = false,
    .implicitCast = false,
    .chainCall = false,
};

struct InlayHint : public ::testing::Test {
protected:
    void run(llvm::StringRef code,
             proto::Range range = {},
             const config::InlayHintOption& option = LikeClangd) {
        tester.emplace("main.cpp", code);
        tester->run();
        auto& info = tester->info;
        SourceConverter converter;
        result = feature::inlayHints({.range = range}, info, converter, option);
    }

    std::optional<Tester> tester;
    proto::InlayHintsResult result;
};

TEST_F(InlayHint, RequestRange) {
    run(R"cpp(
auto x1 = 1;$(request_range_start)
auto x2 = 1;
auto x3 = 1;
auto x4 = 1;$(request_range_end)
)cpp",
        {
            // $(request_range_start)
            .start = {1, 12},
            // $(request_range_end)
            .end = {4, 12},
    },
        {
            .implicitCast = true,
        });

    // 3: x2, x3, x4 is included in the request range.
    EXPECT_EQ(result.size(), 3);
}

TEST_F(InlayHint, AutoDecl) {
    run(R"cpp(
auto$(1) x = 1;

void f() {
    const auto&$(2) x_ref = x;

    if (auto$(3) z = x + 1) {}

    for(auto$(4) i = 0; i<10; ++i) {}
}

template<typename T>
void t() {
    auto z = T{};
}
)cpp");

    EXPECT_EQ(result.size(), 4);
}

TEST_F(InlayHint, FreeFunctionArguments) {
    run(R"cpp(
void f(int a, int b) {}
void g(int a = 1) {}
void h() {
f($(1)1, $(2)2);
g();
}

)cpp");

    EXPECT_EQ(result.size(), 2);
}

TEST_F(InlayHint, FnArgPassedAsLValueRef) {
    run(R"cpp(
void f(int& a, int& b) { }
void g() {
int x = 1;
f($(1)x, $(2)x);
}
)cpp");

    EXPECT_EQ(result.size(), 2);
}

TEST_F(InlayHint, MethodArguments) {
    run(R"cpp(
struct A {
    void f(int a, int b, int d) {}
};

void f() {
    A a;
    a.f($(1)1, $(2)2, $(3)3);
}
)cpp");

    EXPECT_EQ(result.size(), 3);
}

TEST_F(InlayHint, OperatorCall) {
    run(R"cpp(
struct A {
    int operator()(int a, int b) { return a + b; }
};

int f() {
    A a;
    return a(1, 2);
}
)cpp");

    EXPECT_EQ(result.size(), 2);
}

TEST_F(InlayHint, ReturnTypeHint) {
    run(R"cpp(
auto f()$(1) {
    return 1;
}

void g() {
    []()$(2) {
        return 1;
    }();

    [] $(3){
        return 1;
    }();
}

)cpp");

    EXPECT_EQ(result.size(), 3);
}

TEST_F(InlayHint, StructureBinding) {
    run(R"cpp(
int f() {
    int a[2];
    auto [x$(1), y$(2)] = a;
    return x + y; // use x and y to avoid warning.
}
)cpp");

    EXPECT_EQ(result.size(), 2);
}

TEST_F(InlayHint, Constructor) {
    run(R"cpp(
struct A {
    int x;
    int y;

    A(int a, int b):x(a), y(b) {}
};

void f() {
    A a$(1){1, 2};
    A b$(2)(1, 2);
    A c$(3) = {1, 2};
}

)cpp");

    EXPECT_EQ(result.size(), 6);
}

TEST_F(InlayHint, InitializeList) {
    run(R"cpp(
    int a[3] = {1, 2, 3};
    int b[2][3] = {{1, 2, 3}, {4, 5, 6}};
)cpp");

    EXPECT_EQ(result.size(), 3 + (3 * 2 + 2));
}

TEST_F(InlayHint, Designators) {
    run(R"cpp(
struct A{ int x; int y;};
A a = {.x = 1, .y = 2};
)cpp");

    EXPECT_EQ(result.size(), 0);
}

TEST_F(InlayHint, IgnoreSimpleSetter) {
    run(R"cpp(
struct A { 
    void setPara(int Para); 
    void set_para(int para); 
    void set_para_meter(int para_meter); 
};

void f() { 
    A a; 
    a.setPara(1); 
    a.set_para(1); 
    a.set_para_meter(1);
}

)cpp");

    EXPECT_EQ(result.size(), 0);
}

TEST_F(InlayHint, BlockEnd) {
    run(R"cpp(
struct A { 
    int x;
}$(1);

void g() {
}$(2)

namespace no {} // there is no block end hint in a one line defination. 

namespace yes {
}$(3)

namespace yes::nested {
}$(4)

namespace skip {
} // some text here, no hint generated.

struct Out {
    struct In {

    };$(5)}$(6);
)cpp",
        {},
        {
            .blockEnd = true,
            .structSizeAndAlign = false,
        });

    EXPECT_EQ(result.size(), 6);
}

TEST_F(InlayHint, Lambda) {
    run(R"cpp(
auto l = []$(1) {
    return 1;
}$(2);
)cpp",
        {},
        {.returnType = true, .blockEnd = true});

    EXPECT_EQ(result.size(), 3);
}

TEST_F(InlayHint, StructAndMemberHint) {
    run(R"cpp(
struct A {
    int x;
    int y;

    struct B {
        int z;
    };
};
)cpp",
        {},
        {
            .blockEnd = false,
            .structSizeAndAlign = true,
            .memberSizeAndOffset = true,
        });

    /// TODO:
    /// if InlayHintOption::memberSizeAndOffset was implemented, the total hint count is 2 + 3.
    EXPECT_EQ(result.size(), 2 /*+ 3*/);
}

TEST_F(InlayHint, ImplicitCast) {
    run(R"cpp(
    int x = 1.0;
)cpp",
        {},
        {.implicitCast = true});

    /// FIXME: Hint count should be 1.
    EXPECT_EQ(result.size(), 0);
}

}  // namespace
}  // namespace clice::testing
