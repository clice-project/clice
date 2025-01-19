#include "Test/Test.h"
#include "Server/Async.h"

namespace clice::testing {

namespace {

TEST(Async, Task) {
    auto task = []() -> async::Task<int> {
        co_return 1;
    }();

    auto task2 = []() -> async::Task<int> {
        co_return 2;
    }();

    auto task3 = []() -> async::Task<int> {
        co_return 3;
    }();

    auto result = async::run(task, task2, task3);

    EXPECT_EQ(task.done(), 1);
    EXPECT_EQ(task2.done(), 1);
    EXPECT_EQ(task3.done(), 1);
    EXPECT_EQ(result, std::tuple{1, 2, 3});
}

}  // namespace

}  // namespace clice::testing

