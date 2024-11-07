#include "Tester.h"

namespace {

using namespace clice;

TEST(Index, ClassTemplate) {
    const char* code = R"cpp(
    template <typename T, typename U>
    struct $(primary)foo {};

    template <typename T>
    struct $(partial)foo<T, T> {};

    template <>
    struct $(full)foo<int, int> {};

    template struct $(explicit_primary)foo<char, int>;

    template struct $(explicit_partial)foo<char, char>;

    $(full1)foo<int, int> a;
    $(primary1)foo<int, char> b;
    $(primary2)foo<char, int> c;
    $(partial1)foo<char, char> d;
)cpp";

    IndexerTester tester(code, true);
    tester.GotoDefinition("explicit_primary", "primary");
    tester.GotoDefinition("explicit_partial", "partial");
    tester.GotoDefinition("full1", "full");
    tester.GotoDefinition("primary1", "primary");
    tester.GotoDefinition("primary2", "primary");
    tester.GotoDefinition("partial1", "partial");
}

}  // namespace

