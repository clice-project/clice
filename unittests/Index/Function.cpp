#include "Test/IndexTester.h"

namespace clice::testing {

namespace {

TEST(Index, FunctionParams) {
    const char* code = R"cpp(
    void foo(int);

    void bar(int, int y = 2);
)cpp";

    IndexTester tester("main.cpp", code);
    tester.run();

    auto data = tester.indices.find(tester.info->getInterestedFile())->second.toJSON();

    /// TODO: add more tests, FunctionTemplate, VarTemplate, ..., Dependent Name, ..., etc.
    /// add tests for find references ..., !test symbol count.
}

TEST(Index, FunctionType) {
    const char* code = R"cpp(
    template <typename T>
    struct function;

    void bar(function<void(int)>& f);

    void bar2(function<void(int x)>& f);
)cpp";

    IndexTester tester("main.cpp", code);
    tester.run();

    auto data = tester.indices.find(tester.info->getInterestedFile())->second.toJSON();
    /// println("{}", data);
    /// tester.info->tu()->dump();

    /// TODO: add more tests, FunctionTemplate, VarTemplate, ..., Dependent Name, ..., etc.
    /// add tests for find references ..., !test symbol count.
}

}  // namespace

}  // namespace clice::testing

