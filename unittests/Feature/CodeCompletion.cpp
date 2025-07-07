#include "Test/CTest.h"
#include "Feature/CodeCompletion.h"

namespace clice::testing {

namespace {

struct CodeCompletion : TestFixture {
    auto code_complete(llvm::StringRef code) {
        Annotation annotation = {code};
        CompilationParams params;
        std::vector<const char*> arguments = {"clang++", "-std=c++20", "main.cpp"};
        params.arguments = arguments;

        params.completion = {"main.cpp", annotation.offset("pos")};
        params.add_remapped_file("main.cpp", annotation.source());

        config::CodeCompletionOption options = {};
        auto result = feature::code_complete(params, options);
        for(auto& item: result) {
            clice::println("{} {}", refl::enum_name(item.kind), item.label);
        }
        return result;
    }
};

TEST_F(CodeCompletion, Score) {
    using enum feature::CompletionItemKind;
    auto result = code_complete(R"cpp(
int foooo(int x);
int x = fo$(pos)
)cpp");
    ASSERT_EQ(result.size(), 1);
    ASSERT_EQ(result.front().label, "foooo");
    ASSERT_EQ(result.front().kind, Function);
}

TEST_F(CodeCompletion, Snippet) {
    auto result = code_complete(R"cpp(
int x = tru$(pos)
)cpp");
}

TEST_F(CodeCompletion, Overload) {
    auto result = code_complete(R"cpp(
int foooo(int x);
int foooo(int x, int y);
int x = fooo$(pos)
)cpp");
}

TEST_F(CodeCompletion, Unqualified) {
    code_complete(R"cpp(
namespace A { 
    void fooooo(); 
}

void bar() {
    fo$(pos)
}
)cpp");

    /// EXPECT: "A::fooooo"
    /// To implement this we need to search code completion result from index
    /// or traverse AST to collect interesting names.
}

TEST_F(CodeCompletion, Functor) {

    code_complete(R"cpp(
    struct X {
        void operator() () {}
    };
    
void bar() {
    X foo;
    fo$(pos);
}
)cpp");

    /// TODO:
    /// complete lambda as it is a variable.
}

TEST_F(CodeCompletion, Lambda) {
    code_complete(R"cpp(
void bar() {
    auto foo = [](int x){ };
    fo$(pos);
}
)cpp");

    /// TODO:
    /// complete lambda as it is a function.
}

}  // namespace

}  // namespace clice::testing

