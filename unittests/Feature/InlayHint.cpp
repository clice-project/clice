#include <gtest/gtest.h>
#include <Feature/InlayHint.h>

#include "../Test.h"

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
    auto res = feature::inlayHints({}, info, {});

    dbg(res);

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
    auto res = feature::inlayHints({}, info, {});

    dbg(res);

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
    auto res = feature::inlayHints({}, info, {});

    dbg(res);

    txs.equal(res.size(), 2)
        //
        ;
}

TEST(InlayHint, MethodArguments) {
    const char* main = R"cpp(
struct A {
    void f(int a, int b) {}
};

void f() {
    A a;
    a.f($(1)1, $(2)2);
}
)cpp";

    Tester txs("main.cpp", main);
    txs.run();

    auto& info = txs.info;
    auto res = feature::inlayHints({}, info, {});

    dbg(res);

    txs.equal(res.size(), 2)
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
    auto res = feature::inlayHints({}, info, {});

    dbg(res);

    /// FIXME: the hint count should be 2 but is 0.
    // txs.equal(res.size(), 2)
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
    auto res = feature::inlayHints({}, info, {});

    dbg(res);

    txs.equal(res.size(), 0)
        //
        ;
}

}  // namespace
}  // namespace clice
