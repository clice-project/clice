#include "Test/CTest.h"
#include "Feature/Hover.h"

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

    static bool isNone(std::optional<HoverInfo>& hover) {
        bool no = !hover.has_value();
        EXPECT_TRUE(no);
        return no;
    }

    void EXPECT_HOVER_TYPE(llvm::StringRef key,
                           HoverChecker checker,
                           config::HoverOption option = DefaultOption) {
        auto pos = tester->locations.at(key);
        auto hoverInfo = hover(pos.line + 1, pos.character + 1, *tester->info, option);

        bool checkResult = checker(hoverInfo);
        // if(hoverInfo.has_value()) {
        //     llvm::outs() << "======[" << key << "]======\n";
        //     llvm::outs() << toMarkdown(*hoverInfo, option).markdown << '\n';
        // }
        EXPECT_TRUE(checkResult);
    }

    using Fmtter = std::string(int index);

    void EXPECT_TYPES_N(Fmtter fmt, int n, HoverChecker checker) {
        for(int i = 1; i <= n; i++) {
            auto key = fmt(i);
            EXPECT_HOVER_TYPE(key, checker);
        }
    };
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

TEST_F(Hover, HeaderAndNamespace) {
    auto header = R"cpp()cpp";

    auto code = R"cpp(
#in$(h1)clude "head$(h3)er.h"$(h4)
#in$(h2)clude <stddef.h$(h5)>

$(n1)names$(n2)pace$(n3) outt$(n4)er { 

    namespac$(n5)e $(n6){

        nam$(n7)espace inne$(n8)r {

        }$(n9)

    }

}$(n10)

)cpp";

    runWithHeader(code, header);

    EXPECT_TYPES_N([](int i) { return std::format("h{}", i); }, 5, is<Header>);
    EXPECT_TYPES_N([](int i) { return std::format("n{}", i); }, 8, is<Namespace>);
}

TEST_F(Hover, VariableAndLiteral) {
    auto code = R"cpp(
    // introduce size_t
    #include <cstddef>

    long operator ""_w(const char*, size_t) {
        return 1;
    };

    aut$(v1)o$(v2) i$(v3)1$(v4) = $(n1)1$(n2);
    auto$(v5) i2 = $(n3)-$(n4)1$(n5);
 
    auto l1$(v6) = $(l1)"test$(l2)_string_li$(l3)t";
    auto l2 = R$(l4)"tes$(l5)t(raw_string_lit)test"$(l6);
    auto l3 = u8$(l7)"test$(l8)_string_li$(l9)t";
    auto l$(v7)4 = $(l10)"$(l11)udf_string$(l12)"_w;
)cpp";
    run(code);

    EXPECT_TYPES_N([](int i) { return std::format("v{}", i); }, 7, is<Var>);
    EXPECT_TYPES_N([](int i) { return std::format("l{}", i); }, 12, is<Literal>);
}

TEST_F(Hover, FunctionDeclAndParameter) {
    auto code = R"cpp(
    // introduce size_t
    #include <bits/c++config.h>

    i$(f1)nt$(f2) f() { 
        return 0; 
    }

    lo$(f3)ng oper$(f4)ator ""_w(const char* str, std::si$(p1)ze_t$(p2) leng$(p3)th) {$(f5)
        return 1;
    };


    struct A {
        int f$(f6)n(i$(p4)nt par$(p5)am) {
            return param;
        }
        
        voi$(f7)d ope$(f8)rator()$(f9)(int par$(p6)am) {}
    };


    templ$(f10)ate<typenam$(p7)e T1$(p8), typename T$(p9)2>
    void templ$(f11)ate_func1(T1 le$(p10)ft, T2 righ$(p11)t)$(f12) {}

    template<in$(p12)t NonTy$(p13)peParam = 1>
    void templ$(f13)ate_func2() {}

    template<templa$(p14)te<typen$(p15)ame Inn$(p16)er> typenam$(p17)e Outt$(p18)er>
    void templ$(f14)ate_func3() {}

)cpp";
    run(code);

    EXPECT_TYPES_N([](int i) { return std::format("f{}", i); }, 12, is<Fn>);
    EXPECT_TYPES_N([](int i) { return std::format("p{}", i); }, 18, is<Var>);
}

TEST_F(Hover, AutoAndDecltype) {
    auto code = R"cpp(

$(a1)aut$(a2)o$(a3) i = -1;

$(d1)dec$(d2)ltype$(d3)(i) j = 2;

struct A { int x; };

aut$(a4)o va$(a5)r = A{};

a$(fa)uto f1() { return 1; }

de$(fn_decltype)cltype(au$(fn_decltype_auto)to) f2() {}

int f3(au$(fn_para_auto)to x) {}

)cpp";

    run(code);

    EXPECT_TYPES_N([](int i) { return std::format("a{}", i); }, 5, is<Var>);
    EXPECT_TYPES_N([](int i) { return std::format("d{}", i); }, 3, is<Var>);

    EXPECT_HOVER_TYPE("fa", is<Fn>);
    EXPECT_HOVER_TYPE("fn_decltype", is<Fn>);
    EXPECT_HOVER_TYPE("fn_decltype_auto", is<Fn>);

    /// FIXME: It seems a bug of SelectionTree, which cannot select any node of `f3`;
    /// EXPECT_HOVER_TYPE("fn_para_auto", is<Var>);
}

TEST_F(Hover, Expr) {
    auto code = R"cpp(
int xxxx = 1;
int yyyy = xx$(e1)xx;

struct A {
    int function(int param) {
        return thi$(e2)s$(e3)->$(e4)funct$(e5)ion(para$(e6)m);
    }

    int fn(int param) {
        return static$(e7)_cast<A*>(nul$(e8)lptr)->function(par$(e9)am);
    }
};
)cpp";

    run(code);

    EXPECT_HOVER_TYPE("e1", is<Var>);
    EXPECT_HOVER_TYPE("e2", is<Fn>);
    EXPECT_HOVER_TYPE("e3", is<Fn>);
    EXPECT_HOVER_TYPE("e4", is<Fn>);
    EXPECT_HOVER_TYPE("e5", is<Fn>);
    EXPECT_HOVER_TYPE("e6", is<Var>);
    EXPECT_HOVER_TYPE("e7", is<Expression>);
    EXPECT_HOVER_TYPE("e8", is<Expression>);
    EXPECT_HOVER_TYPE("e9", is<Var>);
}

}  // namespace

}  // namespace clice::testing
