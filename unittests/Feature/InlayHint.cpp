#include "Test/CTest.h"
#include "Feature/InlayHint.h"
#include "Basic/SourceConverter.h"

namespace clice::testing {

namespace {

void dbg(const std::vector<proto::InlayHint>& hints) {
    for(auto& hint: hints) {
        llvm::outs() << std::format("kind:{}, position:{}, value_size:{},",
                                    hint.kind.name(),
                                    json::serialize(hint.position),
                                    hint.lable.size());
        for(auto& lable: hint.lable) {
            llvm::outs() << std::format(" value:{}, link position:{}",
                                        lable.value,
                                        json::serialize(lable.Location))
                         << '\n';
        }
    }
}

const SourceConverter Converter{proto::PositionEncodingKind::UTF8};

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
    void compile(llvm::StringRef code) {
        tester.emplace("main.cpp", code);
        tester->run();
    }

    void run(proto::Range range, const config::InlayHintOption& option) {
        auto& info = tester->info;
        SourceConverter converter;
        result = feature::inlayHints({.range = range}, info, converter, option);
    }

    void check(int x, int y) {}

    std::optional<Tester> tester;
    proto::InlayHintsResult result;
};

TEST_F(InlayHint, RequestRange) {
    compile(R"cpp(
auto x1 = 1;$(request_range_start)
auto x2 = 1;
auto x3 = 1;
auto x4 = 1;$(request_range_end)
)cpp");

    run(
        proto::Range{
            .start = {1, 12}, // $(request_range_start)
            .end = {4, 12}, // $(request_range_end)
    },
        {.implicitCast = true});

    // 3: x2, x3, x4 is included in the request range.
    EXPECT_EQ(result.size(), 3);
}

TEST_F(InlayHint, AutoDecl) {
    compile(R"cpp(
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

    run({}, LikeClangd);

    EXPECT_EQ(result.size(), 4);
}

TEST_F(InlayHint, FreeFunctionArguments) {
    compile(R"cpp(
void f(int a, int b) {}
void g(int a = 1) {}
void h() {
f($(1)1, $(2)2);
g();
}

)cpp");

    run({}, LikeClangd);

    EXPECT_EQ(result.size(), 2);
}

TEST_F(InlayHint, FnArgPassedAsLValueRef) {
    compile(R"cpp(
void f(int& a, int& b) { }
void g() {
int x = 1;
f($(1)x, $(2)x);
}
)cpp");

    run({}, LikeClangd);

    EXPECT_EQ(result.size(), 2);
}

TEST_F(InlayHint, MethodArguments) {
    compile(R"cpp(
struct A {
    void f(int a, int b, int d) {}
};

void f() {
    A a;
    a.f($(1)1, $(2)2, $(3)3);
}
)cpp");

    run({}, LikeClangd);

    // dbg(result);

    EXPECT_EQ(result.size(), 3);
}

TEST_F(InlayHint, OperatorCall) {
    compile(R"cpp(
struct A {
    int operator()(int a, int b) { return a + b; }
};

int f() {
    A a;
    return a(1, 2);
}
)cpp");

    run({}, LikeClangd);

    EXPECT_EQ(result.size(), 2);
}

TEST_F(InlayHint, ReturnTypeHint) {
    compile(R"cpp(
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

    run({}, LikeClangd);

    EXPECT_EQ(result.size(), 3);
}

TEST_F(InlayHint, StructureBinding) {
    compile(R"cpp(
int f() {
    int a[2];
    auto [x$(1), y$(2)] = a;
    return x + y; // use x and y to avoid warning.
}
)cpp");

    run({}, LikeClangd);

    EXPECT_EQ(result.size(), 2);
}

TEST_F(InlayHint, Constructor) {
    compile(R"cpp(
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

    run({}, LikeClangd);

    EXPECT_EQ(result.size(), 6);
}

TEST_F(InlayHint, InitializeList) {
    compile(R"cpp(
    int a[3] = {1, 2, 3};
    int b[2][3] = {{1, 2, 3}, {4, 5, 6}};
)cpp");

    run({}, LikeClangd);

    EXPECT_EQ(result.size(), 3 + (3 * 2 + 2));
}

TEST_F(InlayHint, Designators) {
    compile(R"cpp(
struct A{ int x; int y;};
A a = {.x = 1, .y = 2};
)cpp");

    run({}, LikeClangd);

    EXPECT_EQ(result.size(), 0);
}

TEST_F(InlayHint, IgnoreSimpleSetter) {
    compile(R"cpp(
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

    run({}, LikeClangd);

    EXPECT_EQ(result.size(), 0);
}

TEST_F(InlayHint, BlockEnd) {
    compile(R"cpp(
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
)cpp");

    run({},
        {
            .blockEnd = true,
            .structSizeAndAlign = false,
        });

    EXPECT_EQ(result.size(), 6);
}

TEST_F(InlayHint, Lambda) {
    compile(R"cpp(
auto l = []$(1) {
    return 1;
}$(2);
)cpp");

    run({}, {.returnType = true, .blockEnd = true});

    EXPECT_EQ(result.size(), 3);
}

TEST_F(InlayHint, StructAndMemberHint) {
    compile(R"cpp(
struct A {
    int x;
    int y;

    struct B {
        int z;
    };
};
)cpp");

    run({},
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
    compile(R"cpp(
    int x = 1.0;
)cpp");

    run({}, {.implicitCast = true});

    /// FIXME: Hint count should be 1.
    EXPECT_EQ(result.size(), 0);
}

}  // namespace
}  // namespace clice::testing
