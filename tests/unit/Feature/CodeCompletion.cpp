#include "Test/Tester.h"
#include "Feature/CodeCompletion.h"

namespace clice::testing {

namespace {

suite<"CodeCompletion"> code_completion = [] {
    std::vector<feature::CompletionItem> items;

    auto code_complete = [&](llvm::StringRef code) {
        CompilationParams params;
        auto annotation = AnnotatedSource::from(code);
        params.arguments = {"clang++", "-std=c++20", "main.cpp"};
        params.completion = {"main.cpp", annotation.offsets["pos"]};
        params.add_remapped_file("main.cpp", annotation.content);

        config::CodeCompletionOption options = {};
        items = feature::code_complete(params, options);
    };

    using enum feature::CompletionItemKind;

    test("Score") = [&] {
        code_complete(R"cpp(
int foooo(int x);
int x = fo$(pos)
)cpp");
        expect(that % items.size() == 1);
        expect(items.front().label == "foooo");
        expect(items.front().kind == Function);
    };

    test("Snippet") = [&] {
        code_complete(R"cpp(
int x = tru$(pos)
)cpp");
    };

    test("Overload") = [&] {
        code_complete(R"cpp(
int foooo(int x);
int foooo(int x, int y);
int x = fooo$(pos)
)cpp");
    };

    test("Unqualified") = [&] {
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
    };

    test("Functor") = [&] {
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
    };

    test("Lambda") = [&] {
        code_complete(R"cpp(
void bar() {
    auto foo = [](int x){ };
    fo$(pos);
}
)cpp");

        /// TODO:
        /// complete lambda as it is a function.
    };
};

}  // namespace

}  // namespace clice::testing
