#include <gtest/gtest.h>
#include <Feature/InlayHint.h>
#include <Basic/SourceConverter.h>

#include "Test/Test.h"

namespace clice {

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

TEST(InlayHint, RequestRange) {
    const char* main = R"cpp(
auto x1 = 1;$(request_range_start)
auto x2 = 1;
auto x3 = 1;
auto x4 = 1;$(request_range_end)
)cpp";

    Tester txs("main.cpp", main);
    txs.run();
    auto& info = txs.info;

    proto::Range request{
        .start = {1, 12}, // $(request_range_start)
        .end = {4, 12}, // $(request_range_end)
    };
    auto res = feature::inlayHints({.range = request}, info, Converter, {.implicitCast = true});

    // dbg(res);

    // 3: x2, x3, x4 is included in the request range.
    txs.equal(res.size(), 3)
        //
        ;
}

TEST(InlayHint, AutoDecl) {
    const char* main = R"cpp(
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
)cpp";

    Tester txs("main.cpp", main);
    txs.run();

    auto& info = txs.info;
    auto res = feature::inlayHints({}, info, Converter, LikeClangd);

    // dbg(res);

    txs.equal(res.size(), 4)
        //
        ;
}

TEST(InlayHint, FreeFnArguments) {
    const char* main = R"cpp(
void f(int a, int b) {}
void g(int a = 1) {}
void h() {
f($(1)1, $(2)2);
g();
}

)cpp";

    Tester txs("main.cpp", main);
    txs.run();

    auto& info = txs.info;
    auto res = feature::inlayHints({}, info, Converter, LikeClangd);

    // dbg(res);

    txs.equal(res.size(), 2)
        //
        ;
}

TEST(InlayHint, FnArgPassedAsLValueRef) {
    const char* main = R"cpp(
void f(int& a, int& b) { }
void g() {
int x = 1;
f($(1)x, $(2)x);
}
)cpp";

    Tester txs("main.cpp", main);
    txs.run();

    auto& info = txs.info;
    auto res = feature::inlayHints({}, info, Converter, LikeClangd);

    // dbg(res);

    txs.equal(res.size(), 2)
        //
        ;
}

TEST(InlayHint, MethodArguments) {
    const char* main = R"cpp(
struct A {
    void f(int a, int b, int d) {}
};

void f() {
    A a;
    a.f($(1)1, $(2)2, $(3)3);
}
)cpp";

    Tester txs("main.cpp", main);
    txs.run();

    auto& info = txs.info;
    auto res = feature::inlayHints({}, info, Converter, LikeClangd);

    // dbg(res);

    txs.equal(res.size(), 3)
        //
        ;
}

TEST(InlayHint, OperatorCall) {
    const char* main = R"cpp(
struct A {
    int operator()(int a, int b) { return a + b; }
};

int f() {
    A a;
    return a(1, 2);
}
)cpp";

    Tester txs("main.cpp", main);
    txs.run();

    auto& info = txs.info;
    auto res = feature::inlayHints({}, info, Converter, LikeClangd);

    // dbg(res);

    txs.equal(res.size(), 2)
        //
        ;
}

TEST(InlayHint, ReturnTypeHint) {
    const char* main = R"cpp(
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

)cpp";

    Tester txs("main.cpp", main);
    txs.run();

    auto& info = txs.info;
    auto res = feature::inlayHints({}, info, Converter, LikeClangd);

    // dbg(res);

    txs.equal(res.size(), 3)
        //
        ;
}

TEST(InlayHint, StructureBinding) {
    const char* main = R"cpp(
int f() {
    int a[2];
    auto [x$(1), y$(2)] = a;
    return x + y; // use x and y to avoid warning.
}
)cpp";

    Tester txs("main.cpp", main);
    txs.run();

    auto& info = txs.info;
    auto res = feature::inlayHints({}, info, Converter, LikeClangd);

    // dbg(res);

    txs.equal(res.size(), 2)
        //
        ;
}

TEST(InlayHint, Constructor) {
    const char* main = R"cpp(
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

)cpp";

    Tester txs("main.cpp", main);
    txs.run();

    auto& info = txs.info;
    auto res = feature::inlayHints({}, info, Converter, LikeClangd);

    // dbg(res);

    txs.equal(res.size(), 6)
        //
        ;
}

TEST(InlayHint, InitializeList) {
    const char* main = R"cpp(
    int a[3] = {1, 2, 3};
    int b[2][3] = {{1, 2, 3}, {4, 5, 6}};
)cpp";

    Tester txs("main.cpp", main);
    txs.run();

    auto& info = txs.info;
    auto res = feature::inlayHints({}, info, Converter, LikeClangd);

    // dbg(res);

    txs.equal(res.size(), 3 + (3 * 2 + 2))
        //
        ;
}

TEST(InlayHint, Designators) {
    const char* main = R"cpp(
struct A{ int x; int y;};
A a = {.x = 1, .y = 2};
)cpp";

    Tester txs("main.cpp", main);
    txs.run();

    auto& info = txs.info;
    auto res = feature::inlayHints({}, info, Converter, LikeClangd);

    // dbg(res);

    txs.equal(res.size(), 0)
        //
        ;
}

TEST(InlayHint, IgnoreSimpleSetter) {
    const char* main = R"cpp(
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

)cpp";

    Tester txs("main.cpp", main);
    txs.run();

    auto& info = txs.info;
    auto res = feature::inlayHints({}, info, Converter, LikeClangd);

    // dbg(res);

    txs.equal(res.size(), 0)
        //
        ;
}

TEST(InlayHint, BlockEnd) {
    const char* main = R"cpp(
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
)cpp";

    Tester txs("main.cpp", main);
    txs.run();

    config::InlayHintOption option{
        .blockEnd = true,
        .structSizeAndAlign = false,
    };

    auto& info = txs.info;
    auto res = feature::inlayHints({}, info, Converter, option);

    // dbg(res);

    txs.equal(res.size(), 6)
        //
        ;
}

TEST(InlayHint, Lambda) {
    const char* main = R"cpp(
auto l = []$(1) {
    return 1;
}$(2);
)cpp";

    Tester txs("main.cpp", main);
    txs.run();

    auto& info = txs.info;
    auto res = feature::inlayHints({}, info, Converter, {.returnType = true, .blockEnd = true});

    // dbg(res);

    txs.equal(res.size(), 3)
        //
        ;
}

TEST(InlayHint, StructAndMemberHint) {
    const char* main = R"cpp(
struct A {
    int x;
    int y;

    struct B {
        int z;
    };
};
)cpp";

    Tester txs("main.cpp", main);
    txs.run();

    config::InlayHintOption option{
        .blockEnd = false,
        .structSizeAndAlign = true,
        .memberSizeAndOffset = true,
    };

    auto& info = txs.info;
    auto res = feature::inlayHints({}, info, Converter, option);

    // dbg(res);

    /// TODO:
    /// if InlayHintOption::memberSizeAndOffset was implemented, the total hint count is 2 + 3.
    txs.equal(res.size(), 2 /*+ 3*/)
        //
        ;
}

TEST(InlayHint, ImplicitCast) {
    const char* main = R"cpp(
    int x = 1.0;
)cpp";

    Tester txs("main.cpp", main);
    txs.run();
    auto& info = txs.info;
    auto res = feature::inlayHints({}, info, Converter, {.implicitCast = true});

    // dbg(res);

    /// FIXME:
    /// Hint count should be 1.
    txs.equal(res.size(), 0)
        //
        ;
}

}  // namespace
}  // namespace clice
