#include "Test/CTest.h"
#include "Feature/Hover.h"
#include "Basic/SourceConverter.h"

#include <clang/AST/RecursiveASTVisitor.h>

namespace clice::testing {

namespace {

constexpr config::HoverOption DefaultOption = {};

using namespace feature::hover;

struct DeclCollector : public clang::RecursiveASTVisitor<DeclCollector> {

    llvm::StringMap<const clang::Decl*> decls;

    bool VisitNamedDecl(const clang::NamedDecl* decl) {
        assert(decl && "???");
        decls[decl->getName()] = decl;
        return true;
    }
};

struct Hover : public ::testing::Test {
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
    EXPECT_HOVER("Free", FREE_STYLE);
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

}  // namespace

}  // namespace clice::testing
