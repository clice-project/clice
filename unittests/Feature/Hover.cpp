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

    EXPECT_HOVER("A", "### namespace A");
    EXPECT_HOVER("B", "### namespace A::B");
    EXPECT_HOVER("C", "### namespace A::B::C");
    EXPECT_HOVER("D", "### namespace A::B::(anonymous)::D");
    EXPECT_HOVER("std", "### namespace std");

    /// FIXME: inline ?
    // EXPECT_HOVER("E", "### namespace A::(inline)E");
    EXPECT_HOVER("E", "### namespace A::E");

    EXPECT_HOVER("F", "### namespace std::F");
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
From namespace: `out::in`
___
<TODO: document>
___
size: 24 (0x18) bytes, align: 8 (0x8) bytes, 
___
Fields:
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

}  // namespace

}  // namespace clice::testing
