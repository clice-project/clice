#include "IndexTester.h"

namespace clice {

namespace {

TEST(Index, Test) {

    const char* code = R"cpp(
#include <iostream>
)cpp";

    IndexTester tester{"main.cpp", code};
    tester.run();
    auto indices = index::test(tester.info);

    std::size_t total = 0;
    for(auto& [id, index]: indices) {
        auto& srcMgr = tester.info.srcMgr();
        auto entry = srcMgr.getFileEntryRefForID(id);

        llvm::SmallString<128> path;
        auto err = fs::real_path(entry->getName(), path);
        print("File: {}, Size: {}k\n", path.str(), index.size / 1024);
        total += index.size;
    }

    print("Total size: {}k\n", total / 1024);
}

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

    IndexTester tester("main.cpp", code);
    tester.run();
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

    IndexTester tester("main.cpp", code);
    tester.run();

    tester.GotoDefinition("primary_decl", "primary");
    /// FIXME:
    /// tester.GotoDefinition("explicit_primary", "primary");
    tester.GotoDefinition("implicit_primary", "primary");

    tester.GotoDefinition("spec_decl", "spec");
    tester.GotoDefinition("implicit_spec", "spec");
}

TEST(Index, AliasTemplate) {
    const char* code = R"cpp(
    template <typename T>
    using $(primary)foo = T;

    $(implicit_primary)foo<int> a;   
)cpp";

    IndexTester tester("main.cpp", code);
    tester.run();
    tester.GotoDefinition("implicit_primary", "primary");
}

TEST(Index, VarTemplate) {
    const char* code = R"cpp(
    template <typename T, typename U>
    extern int $(primary_decl)foo;

    template <typename T, typename U>
    int $(primary)foo = 1;

    template <typename T>
    extern int $(partial_spec_decl)foo<T, T>;

    template <typename T>
    int $(partial_spec)foo<T, T> = 2;

    template <>
    float $(full_spec)foo<int, int> = 1.0f;

    template int $(explicit_primary)foo<char, int>;

    template int $(explicit_partial)foo<char, char>;

    int main() {
        $(implicit_primary_1)foo<int, char> = 1;
        $(implicit_primary_2)foo<char, int> = 2;
        $(implicit_partial)foo<char, char> = 3;
        $(implicit_full)foo<int, int> = 4;
        return 0;
    }
)cpp";

    IndexTester tester("main.cpp", code);
    tester.run();

    tester.GotoDefinition("primary_decl", "primary");
    /// tester.GotoDefinition("explicit_primary", "primary");
    tester.GotoDefinition("implicit_primary_1", "primary");
    tester.GotoDefinition("implicit_primary_2", "primary");

    tester.GotoDefinition("partial_spec_decl", "partial_spec");
    /// tester.GotoDefinition("explicit_partial", "partial_spec");
    tester.GotoDefinition("implicit_partial", "partial_spec");

    tester.GotoDefinition("implicit_full", "full_spec");
}

TEST(Index, Concept) {
    const char* code = R"cpp(
    template <typename T>
    concept $(primary)foo = true;

    static_assert($(implicit)foo<int>);

    $(implicit2)foo auto bar = 1;
)cpp";

    IndexTester tester("main.cpp", code);
    tester.run();

    tester.GotoDefinition("primary", "primary");
    tester.GotoDefinition("implicit", "primary");
    tester.GotoDefinition("implicit2", "primary");
}

}  // namespace

}  // namespace clice

