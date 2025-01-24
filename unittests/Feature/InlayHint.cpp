#include "Test/CTest.h"
#include "Feature/InlayHint.h"
#include "Basic/SourceConverter.h"

namespace clice::testing {

namespace {

constexpr config::InlayHintOption LikeClangd{
    .maxLength = 20,
    .maxArrayElements = 10,
    .structSizeAndAlign = false,
    .memberSizeAndOffset = false,
    .implicitCast = false,
    .chainCall = false,
};

bool operator== (const proto::Position& lhs, const proto::Position rhs) {
    return lhs.character == rhs.character && lhs.line == rhs.line;
}

auto operator<=> (const proto::Position& lhs, const proto::Position rhs) {
    return std::tie(lhs.line, lhs.character) <=> std::tie(rhs.line, rhs.character);
}

using namespace feature::inlay_hint;

struct InlayHints : public ::testing::Test {
protected:
    std::optional<Tester> tester;
    Result result;

    void run(llvm::StringRef code, proto::Range range = {},
             const config::InlayHintOption& option = LikeClangd) {
        tester.emplace("main.cpp", code);
        tester->run();
        auto& info = tester->info;
        SourceConverter converter;
        result = inlayHints({.range = range}, *info, converter, option);
    }

    size_t indexOf(llvm::StringRef key) {
        auto expect = tester->offset(key);
        auto iter = std::find_if(result.begin(), result.end(), [&expect](InlayHint& hint) {
            return hint.offset == expect;
        });

        assert(iter != result.end());
        return std::distance(result.begin(), iter);
    }

    void EXPECT_AT(llvm::StringRef key, llvm::StringRef text) {
        auto index = indexOf(key);
        EXPECT_EQ(result[index].lable.value, text);
    }

    void EXPECT_ALL_KEY_IS_TEXT() {
        EXPECT_EQ(tester->locations.size(), result.size());

        for(auto& hint: result) {
            auto text = hint.lable.value;
            auto expect = tester->offset(text);
            EXPECT_EQ(expect, hint.offset);
        }
    }

    void EXPECT_HINT_COUNT(size_t count, llvm::StringRef startKey = "",
                           llvm::StringRef endKey = "") {
        size_t begin = 0;
        size_t end = result.size();

        if(!startKey.empty()) {
            auto left = tester->offset(startKey);
            begin = std::count_if(result.begin(), result.end(), [left](const InlayHint& hint) {
                return hint.offset <= left;
            });
        }

        if(!endKey.empty()) {
            auto right = tester->offset(startKey);
            begin = std::count_if(result.begin(), result.end(), [right](const InlayHint& hint) {
                return hint.offset <= right;
            });
        }

        EXPECT_EQ(count, end - begin);
    }
};

TEST_F(InlayHints, MaxLength) {
    run(R"cpp(
    struct _2345678 {};
    constexpr _2345678 f() { return {}; }

    auto x$(: _2...) = f();
)cpp",
        {},
        {
            .maxLength = 7,
            .structSizeAndAlign = false,
        });

    EXPECT_HINT_COUNT(1);
    EXPECT_ALL_KEY_IS_TEXT();
}

TEST_F(InlayHints, RequestRange) {
    run(R"cpp(
auto x1 = 1;$(request_range_start)
auto x2$(1) = 1;
auto x3$(2) = 1;
auto x4$(3) = 1;$(request_range_end)
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
    EXPECT_HINT_COUNT(3, "request_range_start", "request_range_end");

    auto text = ": int";
    EXPECT_AT("1", text);
    EXPECT_AT("2", text);
    EXPECT_AT("3", text);
}

TEST_F(InlayHints, AutoDecl) {
    run(R"cpp(
auto x$(1) = 1;

void f() {
    const auto& x_ref$(2) = x;

    if (auto z$(3) = x + 1) {}

    for(auto i$(4) = 0; i<10; ++i) {}
}

template<typename T>
void t() {
    auto z = T{};
}
)cpp");

    EXPECT_HINT_COUNT(4);

    auto intHint = ": int";
    auto intRefHint = ": const int &";
    EXPECT_AT("1", intHint);
    EXPECT_AT("2", intRefHint);
    EXPECT_AT("3", intHint);
    EXPECT_AT("4", intHint);
}

TEST_F(InlayHints, FreeFunctionArguments) {
    run(R"cpp(
void f(int a, int b) {}
void g(int a = 1) {}
void h() {
f($(a:)1, $(b:)2);
g();
}

)cpp");

    EXPECT_ALL_KEY_IS_TEXT();
}

TEST_F(InlayHints, FnArgPassedAsLValueRef) {
    run(R"cpp(
void f(int& a, int& b) { }
void g() {
int x = 1;
f($(a&:)x, $(b&:)x);
}
)cpp");

    EXPECT_ALL_KEY_IS_TEXT();
}

TEST_F(InlayHints, MethodArguments) {
    run(R"cpp(
struct A {
    void f(int a, double b, unsigned int c) {}
};

void f() {
    A a;
    a.f($(a:)1, $(b:)2, $(c:)3);
}
)cpp");

    EXPECT_ALL_KEY_IS_TEXT();
}

TEST_F(InlayHints, OperatorCall) {
    run(R"cpp(
struct A {
    int operator()(int a, int b) { return a + b; }
    bool operator <= (const A& rhs) { return false; } 
};

int f() {
    A a, b;

    bool s = a <= b; // should be ignored 
    return a($(a:)1, $(b:)2);
}
)cpp");

    EXPECT_ALL_KEY_IS_TEXT();
}

TEST_F(InlayHints, ReturnTypeHint) {
    run(R"cpp(
auto f()$(-> int) {
    return 1;
}

void g() {
    []()$(-> double) {
        return static_cast<double>(1.0);
    }();

    []$(-> void) {
        return;
    }();
}

)cpp");

    EXPECT_ALL_KEY_IS_TEXT();
}

TEST_F(InlayHints, StructureBinding) {
    run(R"cpp(
int f() {
    int a[2];
    auto [x$(1), y$(2)] = a;
    return x + y; // use x and y to avoid warning.
}
)cpp");

    EXPECT_HINT_COUNT(2);
    EXPECT_AT("1", ": int");
    EXPECT_AT("2", ": int");
}

TEST_F(InlayHints, Constructor) {
    run(R"cpp(
struct A {
    int x;
    float y;

    A(int a, float b):x(a), y(b) {}
};

void f() {
    A a{$(1)1, $(2)2};
    A b($(3)1, $(4)2);
    A c = {$(5)1, $(6)2};
}

)cpp");

    EXPECT_HINT_COUNT(6);

    auto asA = "a:";
    auto asB = "b:";

    EXPECT_AT("1", asA);
    EXPECT_AT("3", asA);
    EXPECT_AT("5", asA);

    EXPECT_AT("2", asB);
    EXPECT_AT("4", asB);
    EXPECT_AT("6", asB);
}

TEST_F(InlayHints, InitializeList) {
    run(R"cpp(
    int a[3] = {1, 2, 3};
    int b[2][3] = {{1, 2, 3}, {4, 5, 6}};
)cpp");

    EXPECT_HINT_COUNT(3 + (3 * 2 + 2));
}

TEST_F(InlayHints, Designators) {
    run(R"cpp(
struct A{ int x; int y;};
A a = {.x = 1, .y = 2};
)cpp");

    EXPECT_HINT_COUNT(0);
}

TEST_F(InlayHints, IgnoreCases) {
    // Ignore
    // 1. simple setters
    // 2. arguments that has had-written /*argName*/
    run(R"cpp(
struct A { 
    void setPara(int Para); 
    void set_para(int para); 
    void set_para_meter(int para_meter); 
};

void f(int name) {}

void g() { 
    A a; 
    a.setPara(1); 
    a.set_para(1); 
    a.set_para_meter(1);

    f(/*name=*/1);
}

)cpp");

    EXPECT_HINT_COUNT(0);
}

TEST_F(InlayHints, BlockEnd) {
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

    };}$(5);
)cpp",
        {},
        {
            .blockEnd = true,
            .structSizeAndAlign = false,
        });

    EXPECT_HINT_COUNT(5);

    EXPECT_AT("1", "// struct A");
    EXPECT_AT("2", "// void g()");
    EXPECT_AT("3", "// namespace yes");
    EXPECT_AT("4", "// namespace nested");
    EXPECT_AT("5", "// struct Out");
}

TEST_F(InlayHints, Lambda) {
    run(R"cpp(
auto l$(1) = []$(2) {
    return 1;
}$(3);
)cpp",
        {},
        {.returnType = true, .blockEnd = true});

    EXPECT_HINT_COUNT(3);

    EXPECT_AT("1", ": (lambda)");
    EXPECT_AT("2", "-> int");
    EXPECT_AT("3", "// lambda #0");
}

TEST_F(InlayHints, StructAndMemberHint) {
    run(R"cpp(
struct A$(size: 8, align: 4) {
    int x;
    int y;

    class B$(size: 4, align: 4) {
        int z;
    };

    enum class C {
        _1,
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
    EXPECT_HINT_COUNT(2 /*+ 3*/);

    EXPECT_ALL_KEY_IS_TEXT();
}

TEST_F(InlayHints, ImplicitCast) {
    run(R"cpp(
    int x = 1.0;
)cpp",
        {},
        {.implicitCast = true});

    /// FIXME: Hint count should be 1.
    EXPECT_HINT_COUNT(0);
}

TEST_F(InlayHints, WithHeaderContext) {
    const char* header = R"cpp(
namespace _1 {
    // there are 2 hint in header, namespace end and type hint of `x`

    struct _2345678 {};
    constexpr _2345678 f() { return {}; }
    constexpr auto x = f();
}

)cpp";

    const char* source = R"cpp(
#include "header.h"

namespace _2 {
    // only one hint here.
}

)cpp";

    Tester tx;
    tx.addFile(path::join(".", "header.h"), header);
    tx.addMain("main.cpp", source);
    tx.run();

    auto& info = tx.info;
    EXPECT_TRUE(info.has_value());

    SourceConverter cvtr{proto::PositionEncodingKind::UTF8};
    auto maps = inlayHints("", *info, cvtr);

    // 2 fileID
    EXPECT_EQ(maps.size(), 2);

    clang::FileID mainID = info->srcMgr().getMainFileID();
    config::InlayHintOption option{
        .blockEnd = true,
    };
    for(auto& [fid, result]: maps) {
        if(fid == mainID) {
            EXPECT_EQ(result.size(), 1);
        } else {
            /// Drop  `structSizeAndAlign` of `struct _2345678`.
            config::InlayHintOption fixOption{
                .blockEnd = true,
                .structSizeAndAlign = false,
            };
            auto lspRes = toLspType(result, "", fixOption, header, cvtr);
            EXPECT_EQ(lspRes.size(), 2);
        }
    }
}

}  // namespace
}  // namespace clice::testing
