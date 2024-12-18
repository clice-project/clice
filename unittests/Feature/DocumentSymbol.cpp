#include <gtest/gtest.h>
#include <Feature/DocumentSymbol.h>

#include "../Test.h"

namespace clice {

namespace {

void dbg(const proto::DocumentSymbolResult& result, size_t ident = 0) {
    for(auto& item: result) {
        for(size_t i = 0; i < ident; ++i)
            llvm::outs() << ' ';
        llvm::outs() << std::format("kind: {}, name:{}, detail:{}, deprecated:{}, children_num:{}",
                                    item.kind.name(),
                                    item.name,
                                    item.detail,
                                    item.deprecated,
                                    item.children.size())
                     << '\n';

        dbg(item.children, ident + 2);
    }
}

void total_size(const proto::DocumentSymbolResult& result, size_t& size) {
    for(auto& item: result) {
        ++size;
        total_size(item.children, size);
    }
}

size_t total_size(const proto::DocumentSymbolResult& result) {
    size_t size = 0;
    total_size(result, size);
    return size;
}

TEST(DocumentSymbol, Namespace) {
    const char* main = R"cpp(
namespace _1 {
    namespace _2 {
    
    }
}

namespace _1 {
    namespace _2 {
        namespace _3 {
        }
    }
}

namespace {}

namespace _1::_2{
}

)cpp";

    Tester txs("main.cpp", main);
    txs.run();

    auto& info = txs.info;
    proto::DocumentSymbolParams param;
    auto res = feature::documentSymbol(param, info);
    // dbg(res);
    ASSERT_EQ(total_size(res), 8);
}

TEST(DocumentSymbol, Struct) {
    const char* main = R"cpp(
struct _1 {};
struct _2 {};

struct _3 {
    struct _4 {};
    struct _5 {};
};

int main(int argc, char* argv[]) {
    struct {
        int x;
        int y;
    } point;

    int local = 0; // no symbol for `local` variable
}

)cpp";

    Tester txs("main.cpp", main);
    txs.run();

    auto& info = txs.info;
    proto::DocumentSymbolParams param;
    auto res = feature::documentSymbol(param, info);
    // dbg(res);
    ASSERT_EQ(total_size(res), 9);
}

TEST(DocumentSymbol, Field) {
    const char* main = R"cpp(

struct x {
    int x1;
    int x2;

    struct y {
        int y1;
        int y2;
    };

    static int x3;
};

)cpp";

    Tester txs("main.cpp", main);
    txs.run();

    auto& info = txs.info;
    proto::DocumentSymbolParams param;
    auto res = feature::documentSymbol(param, info);
    // dbg(res);
    ASSERT_EQ(total_size(res), 7);
}

TEST(DocumentSymbol, Constructor) {
    const char* main = R"cpp(
struct S {
    int x;

    S(): x(0) {}
    S(int x) : x(x) {}
    S(const S& s) : x(s.x) {}
    ~S() {}
};
)cpp";

    Tester txs("main.cpp", main);
    txs.run();

    auto& info = txs.info;
    proto::DocumentSymbolParams param;
    auto res = feature::documentSymbol(param, info);
    // dbg(res);
    ASSERT_EQ(total_size(res), 6);
}

TEST(DocumentSymbol, Method) {
    const char* main = R"cpp(

struct _0 {
    void f(int x) {}
    void f(int* x) {}
    void f1(int& x) {}
    void f2(const int& x) {}
    void f2(const _0& x) {}
    void f2(_0 x) {}
};

)cpp";

    Tester txs("main.cpp", main);
    txs.run();

    auto& info = txs.info;
    proto::DocumentSymbolParams param;
    auto res = feature::documentSymbol(param, info);
    // dbg(res);
    ASSERT_EQ(total_size(res), 7);
}

TEST(DocumentSymbol, Enum) {
    const char* main = R"cpp(

enum class A {
    _1,
    _2,
    _3,
};

enum B {
    _a,
    _b,
    _c,
};

)cpp";

    Tester txs("main.cpp", main);
    txs.run();

    auto& info = txs.info;
    proto::DocumentSymbolParams param;
    auto res = feature::documentSymbol(param, info);
    // dbg(res);
    ASSERT_EQ(total_size(res), 8);
}

TEST(DocumentSymbol, TopLevelVariable) {
    const char* main = R"cpp(
constexpr auto x = 1;
int y = 2;
)cpp";

    Tester txs("main.cpp", main);
    txs.run();

    auto& info = txs.info;
    proto::DocumentSymbolParams param;
    auto res = feature::documentSymbol(param, info);
    // dbg(res);
    ASSERT_EQ(total_size(res), 2);
}

}  // namespace

}  // namespace clice
