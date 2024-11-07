#include "Tester.h"

namespace {

using namespace clice;

TEST(Index, ClassTemplate) {
    const char* code = R"cpp(
    template <typename T, typename U>
    struct $(primary_decl)foo;

    using type = $(forward_full)foo<int, int>;

    template <typename T, typename U>
    struct $(primary)foo {};

    template <typename T>
    struct $(partial_spec_decl)foo<T, T>;

    template <typename T>
    struct $(partial_spec)foo<T, T> {};

    template <>
    struct $(full_spec_decl)foo<int, int>;

    template <>
    struct $(full_spec)foo<int, int> {};

    template struct $(explicit_primary)foo<char, int>;

    template struct $(explicit_partial)foo<char, char>;

    $(implicit_primary_1)foo<int, char> b;
    $(implicit_primary_2)foo<char, int> c;
    $(implicit_partial)foo<char, char> d;
    $(implicit_full)foo<int, int> a;
)cpp";

    IndexerTester tester(code);
    tester.GotoDefinition("primary_decl", "primary");
    tester.GotoDefinition("explicit_primary", "primary");
    tester.GotoDefinition("implicit_primary_1", "primary");
    tester.GotoDefinition("implicit_primary_2", "primary");

    tester.GotoDefinition("partial_spec_decl", "partial_spec");
    tester.GotoDefinition("explicit_partial", "partial_spec");
    tester.GotoDefinition("implicit_partial", "partial_spec");

    tester.GotoDefinition("forward_full", "full_spec");
    tester.GotoDefinition("full_spec_decl", "full_spec");
    tester.GotoDefinition("implicit_full", "full_spec");

    /// TODO: add more tests, FunctionTemplate, VarTemplate, ..., Dependent Name, ..., etc.
    /// add tests for find references ..., !test symbol count.
}

TEST(Index, FunctionTemplate) {
    /// Function template doesn't have partial specialization.
    const char* code = R"cpp(
    template <typename T> void $(primary_decl)foo();

    template <typename T> void $(primary)foo() {}
    
    template <> void $(spec_decl)foo<int>(); 
    
    template <> void $(spec)foo<int>() {}

    template void $(explicit_primary)foo<char>();

    int main() {
        $(implicit_primary)foo<char>();
        $(implicit_spec)foo<int>();
    }
)cpp";

    IndexerTester tester(code, true);
    tester.GotoDefinition("primary_decl", "primary");
    tester.GotoDefinition("explicit_primary", "primary");
    tester.GotoDefinition("implicit_primary", "primary");

    tester.GotoDefinition("spec_decl", "spec");
    tester.GotoDefinition("implicit_spec", "spec");
}

}  // namespace

