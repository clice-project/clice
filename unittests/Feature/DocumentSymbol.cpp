#include "Test/CTest.h"
#include "Server/SourceConverter.h"
#include "Feature/DocumentSymbol.h"

namespace clice::testing {

namespace {

struct DocumentSymbol : Test {

protected:
    auto run(llvm::StringRef code) {
        addMain("main.cpp", code);
        Tester::run();
        EXPECT_TRUE(info.has_value());

        return feature::documentSymbols(*info);
    }

    static void total_size(const std::vector<feature::DocumentSymbol>& result, size_t& size) {
        for(auto& item: result) {
            ++size;
            total_size(item.children, size);
        }
    }

    static size_t total_size(const std::vector<feature::DocumentSymbol>& result) {
        size_t size = 0;
        total_size(result, size);
        return size;
    }
};

TEST_F(DocumentSymbol, Namespace) {
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

    auto res = run(main);
    EXPECT_EQ(total_size(res), 8);
}

TEST_F(DocumentSymbol, Struct) {
    const char* main = R"cpp(
struct _1 {};
struct _2 {};

struct _3 {
    struct _4 {};
    struct _5 {};
};



)cpp";

    auto res = run(main);
    EXPECT_EQ(total_size(res), 5);
    // tester->info->tu()->dump();
    // println("{}", pretty_dump(res));
}

TEST_F(DocumentSymbol, Field) {
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

    auto res = run(main);
    EXPECT_EQ(total_size(res), 7);
}

TEST_F(DocumentSymbol, Constructor) {
    const char* main = R"cpp(
struct S {
    int x;

    S(): x(0) {}
    S(int x) : x(x) {}
    S(const S& s) : x(s.x) {}
    ~S() {}
};
)cpp";
    auto res = run(main);
    EXPECT_EQ(total_size(res), 6);
}

TEST_F(DocumentSymbol, Method) {
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

    auto res = run(main);
    EXPECT_EQ(total_size(res), 7);
}

TEST_F(DocumentSymbol, Enum) {
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

    auto res = run(main);
    EXPECT_EQ(total_size(res), 8);
}

TEST_F(DocumentSymbol, TopLevelVariable) {
    const char* main = R"cpp(
constexpr auto x = 1;
int y = 2;
)cpp";

    auto res = run(main);
    EXPECT_EQ(total_size(res), 2);
}

TEST_F(DocumentSymbol, Macro) {
    const char* main = R"cpp(
#define CLASS(X) class X 

CLASS(test) {
    int x = 1;
};

#define VAR(X) int X = 1; 
VAR(test)

)cpp";

    auto res = run(main);

    // clang-format off

/// FIXME:
/// Fix range for macro expansion.  Current out put is:
//
// debug(res);
// 
// kind: Class, name:test, detail:, deprecated:false, range: {"end":{"character":0,"line":5},"start":{"character":0,"line":3}}, children_num:1
//  kind: Field, name:x, detail:int, deprecated:false, range: {"end":{"character":3,"line":4},"start":{"character":4,"line":4}}, children_num:0
// kind: Variable, name:test, detail:int, deprecated:false, range: {"end":{"character":0,"line":8},"start":{"character":0,"line":8}}, children_num:0

    // clang-format on

    /// EXPECT_EQ(total_size(res), 3);
}

TEST_F(DocumentSymbol, WithHeader) {
    const char* header = R"cpp(

struct Test {
    
    struct Inner {
       double z;
    };

    int a;

    constexpr static int x = 1;
};

)cpp";

    const char* main = R"cpp(
#include "header.h"

constexpr auto x = 1;
int y = 2;
)cpp";

    Tester tx;
    tx.addFile(path::join(".", "header.h"), header);
    tx.addMain("main.cpp", main);
    tx.run();

    auto& info = tx.info;
    EXPECT_TRUE(info.has_value());

    auto maps = feature::indexDocumentSymbols(*info);
    for(auto& [fileID, result]: maps) {
        if(fileID == info->srcMgr().getMainFileID()) {
            EXPECT_EQ(total_size(result), 2);
        } else {
            EXPECT_EQ(total_size(result), 5);
        }
    }
}

}  // namespace

}  // namespace clice::testing
