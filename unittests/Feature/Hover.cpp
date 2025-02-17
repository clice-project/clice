#include "Test/CTest.h"
#include "Feature/Hover.h"
#include "Basic/SourceConverter.h"

#include "src/Feature/Hover.cpp"

#include <clang/AST/RecursiveASTVisitor.h>

namespace clice::testing {

namespace {

constexpr config::HoverOption DefaultOption = {};

using namespace feature::hover;

struct DeclCollector : public clang::RecursiveASTVisitor<DeclCollector> {

    llvm::StringMap<const clang::Decl*> decls;

    bool VisitNamedDecl(const clang::NamedDecl* decl) {
        assert(decl && "Decl must be a valid pointer");
        decls[decl->getNameAsString()] = decl;
        return true;
    }
};

struct Hover : public ::testing::Test {
    using HoverChecker = llvm::function_ref<bool(std::optional<HoverInfo>&)>;

protected:
    std::optional<Tester> tester;

    llvm::StringMap<const clang::Decl*> decls;

    void run(llvm::StringRef code,
             proto::Range range = {},
             const config::HoverOption& option = DefaultOption) {
        tester.emplace("main.cpp", code);
        tester->run();

        auto& info = tester->info;

        DeclCollector collector;
        collector.TraverseTranslationUnitDecl(info->tu());
        decls = std::move(collector.decls);
    }

    void runWithHeader(llvm::StringRef source,
                       llvm::StringRef header,
                       const config::HoverOption& option = DefaultOption) {
        tester.emplace("main.cpp", source);
        tester->addFile(path::join(".", "header.h"), header);
        tester->run();

        auto& info = tester->info;
        DeclCollector collector;
        collector.TraverseTranslationUnitDecl(info->tu());
        decls = std::move(collector.decls);
    }

    const clang::Decl* getValidDeclPtr(llvm::StringRef name) {
        auto ptr = decls.lookup(name);
        EXPECT_TRUE(bool(ptr));
        return ptr;
    }

    void EXPECT_HOVER(llvm::StringRef declName,
                      llvm::function_ref<bool(const Result&)> checker,
                      const config::HoverOption& option = DefaultOption) {
        auto ptr = getValidDeclPtr(declName);
        auto result = hover(ptr, option);
        EXPECT_TRUE(checker(result));
    }

    void EXPECT_HOVER(llvm::StringRef declName,
                      llvm::StringRef mdText,
                      const config::HoverOption& option = DefaultOption) {
        auto ptr = getValidDeclPtr(declName);
        auto result = hover(ptr, option);

        // llvm::outs() << result.markdown << '\n';
        EXPECT_EQ(mdText, result.markdown);
    }

    template <typename T>
    static bool is(std::optional<HoverInfo>& hover) {
        EXPECT_TRUE(hover.has_value());
        if(hover.has_value()) {
            return std::holds_alternative<T>(*hover);
        }
        return false;
    }

    void EXPECT_HOVER_TYPE(llvm::StringRef key,
                           HoverChecker checker,
                           config::HoverOption option = DefaultOption) {
        SourceConverter cvtr{};
        auto position = tester->locations.at(key);
        auto hoverInfo = hover(position, *tester->info, cvtr, option);

        bool checkResult = checker(hoverInfo);
        // if(hoverInfo.has_value()) {
        //     llvm::outs() << toMarkdown(*hoverInfo, option).markdown << '\n';
        // }
        EXPECT_TRUE(checkResult);
    }
};

TEST_F(Hover, Namespace) {
    run(R"cpp(
namespace A {
    namespace B {
        namespace C {}
        
        /// anonymous namespace 
        namespace {
            namespace D {}
        }
    }

    inline namespace E {}
}


/// std namespace
namespace std {
    namespace F {}
}
)cpp");

    // EXPECT_HOVER("", "");
    // EXPECT_HOVER("", "");
    // EXPECT_HOVER("", "");
    // EXPECT_HOVER("", "");
    // EXPECT_HOVER("", "");
    // EXPECT_HOVER("", "");
    // EXPECT_HOVER("", "");

    /// FIXME: inline ?
    // EXPECT_HOVER("E", "### namespace A::(inline)E");
}

TEST_F(Hover, RecordScope) {
    run(R"cpp(
typedef struct A {
    struct B {
        struct C {};
    };

    struct {
        struct D {};
    } _;
} T;

/// forward declaration 
struct FORWARD_STRUCT;
struct FORWARD_CLASS;

/// in function body 
void f() {
    struct X {};

    class Y {};

    struct {
        struct Z {};
    } _;
}

namespace n1 {

    namespace n2 {
        struct NA {
            struct NB {};
        };
    }
    
    namespace {
        struct NC {};
    }
}

namespace out {
    namespace in {
        struct M {
            int x;
            double y;
            char z;
            T a, b;
        };
    }
}

)cpp");

    // EXPECT_HOVER("A", "");
    // EXPECT_HOVER("B", "");
    // EXPECT_HOVER("C", "");

    // EXPECT_HOVER("X", "");
    // EXPECT_HOVER("Y", "");
    // EXPECT_HOVER("Z", "");

    // EXPECT_HOVER("NA", "");
    // EXPECT_HOVER("NB", "");
    // EXPECT_HOVER("NC", "");

    auto M_TEXT = R"md(### Struct `M`

In namespace: `out::in`

___
<TODO: document>

___
size: 24 (0x18) bytes, align: 8 (0x8) bytes, 
___
5 fields:

+ x: `int`

+ y: `double`

+ z: `char`

+ a: `T` (aka `struct A`)

+ b: `T` (aka `struct A`)

___
<TODO: source code>
)md";
    EXPECT_HOVER("M", M_TEXT);
}

TEST_F(Hover, EnumStyle) {
    run(R"cpp(

enum Free {
    A = 1,
    B = 2,
    C = 999,
};

enum class Scope: long {
    A = -8,
    B = 2,
    C = 100,
};

)cpp");

    auto FREE_STYLE = R"md(### Enum `Free` `(unsigned int)`

In namespace: `(global)`, (unscoped)

___
<TODO: document>

___
3 items:

+ A = `1 (0x1)`

+ B = `2 (0x2)`

+ C = `999 (0x3E7)`

___
<TODO: source code>
)md";

#ifndef _WIN32
    // The underlying type of `Free` is `int` on Windows.
    EXPECT_HOVER("Free", FREE_STYLE);
#endif

    // EXPECT_HOVER("Scope", "");
}

TEST_F(Hover, FunctionStyle) {
    run(R"cpp(

typedef long long ll;

ll f(int x, int y, ll z = 1) { return 0; }

template<typename T, typename S>
T t(T a, T b, int c, ll d, S s) { return a; }

namespace {
    constexpr static const char* g() { return "hello"; }
}

namespace test {
    namespace {
        [[deprecated("test deprecate message")]] consteval int h() { return 1; }
    }
}

struct A {
    constexpr static A m(int left, double right) { return A(); }
};

)cpp");

    auto FUNC_STYLE = R"md(### Method `m`

In namespace: `(global)`, scope: `A`

___
`constexpr` `inline` `static`

___
-> `A` (aka `struct A`)

___
2 parameters:

+ left: `int`

+ right: `double`

___
<TODO: document>

___
<TODO: source code>
)md";
    // EXPECT_HOVER("f", FREE_STYLE);
    // EXPECT_HOVER("t", FREE_STYLE);
    // EXPECT_HOVER("g", FREE_STYLE);
    // EXPECT_HOVER("h", FREE_STYLE);

    EXPECT_HOVER("m", FUNC_STYLE);
}

TEST_F(Hover, VariableStyle) {
    run(R"cpp(

void f() {
    constexpr static auto x1 = 1;
}
)cpp");

    auto FREE_STYLE = R"md(### Variable `x1`

In namespace: `(global)`, scope: `f`

___
`constexpr` `static` `(local variable)`

Type: `const int`

size = 4 bytes, align = 4 bytes

___
<TODO: document>

___
<TODO: source code>
)md";

    EXPECT_HOVER("x1", FREE_STYLE);
}

TEST_F(Hover, HoverCase) {
    auto header = R"cpp(
int f();

namespace n {
    int f(int x);
}

)cpp";

    auto code = R"cpp($(e1)
#in$(h1)clude "head$(h3)er.h"$(h4)
#in$(h2)clude <stddef.h$(h5)>

$(e2)

i$(k1)nt$(k2) f() { 
    return 0; 
}$(e3)
$(e4)

aut$(k3)o$(k4) i1 = $(n1)1$(n2);
auto i2 = $(n3)-$(n4)1$(n5);


long oper$(k5)ator ""_w(const char*, uns$(k6)igned$(k7) long) {
    return 1;
};

auto l1 = $(l1)"test$(l2)_string_li$(l3)t";
auto l2 = R$(l4)"tes$(l5)t(raw_string_lit)test"$(l6);
auto l3 = u8$(l7)"test$(l8)_string_li$(l9)t";
auto l4 = "test$(l10)_string_li$(l11)t_udf"_w;

$(k8)names$(k9)pace$(k10) n {  $(e5)
    int f(int x) {
    $(e6)    return static_cast<int>($(n6)13$(n7)7.1$(n8)5$(n9));   $(e7)
    }
}

$(e8)
)cpp";

    runWithHeader(code, header);

    using Fmtter = std::string(int index);
    auto EXPECT_TYPES_N = [this](Fmtter fmt, int n, HoverChecker checker) {
        for(int i = 1; i <= n; i++) {
            auto key = fmt(i);
            EXPECT_HOVER_TYPE(key, checker);
        }
    };

    auto isNone = [](std::optional<HoverInfo>& hover) -> bool {
        return !hover.has_value();
    };

    EXPECT_TYPES_N([](int i) { return std::format("e{}", i); }, 8, isNone);
    EXPECT_TYPES_N([](int i) { return std::format("h{}", i); }, 5, is<Header>);
    EXPECT_TYPES_N([](int i) { return std::format("n{}", i); }, 8, is<Numeric>);
    EXPECT_TYPES_N([](int i) { return std::format("l{}", i); }, 11, is<Literal>);
    EXPECT_TYPES_N([](int i) { return std::format("k{}", i); }, 10, is<Keyword>);
}

}  // namespace

}  // namespace clice::testing
