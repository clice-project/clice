#include "Test/Test.h"
#include "Async/Async.h"
#include <thread>

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

    EXPECT_EQ(task.done(), true);
    EXPECT_EQ(task2.done(), true);
    EXPECT_EQ(task3.done(), true);
    EXPECT_EQ(result, std::tuple{1, 2, 3});
}

TEST(Async, Submit) {
    auto task = []() -> async::Task<std::thread::id> {
        co_return co_await async::submit([]() {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            return std::this_thread::get_id();
        });
    }();

    auto task2 = []() -> async::Task<std::thread::id> {
        co_return co_await async::submit([]() {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            return std::this_thread::get_id();
        });
    }();

    auto task3 = []() -> async::Task<std::thread::id> {
        co_return co_await async::submit([]() {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            return std::this_thread::get_id();
        });
    }();

    auto result = async::run(task, task2, task3);

    EXPECT_EQ(task.done(), true);
    EXPECT_EQ(task2.done(), true);
    EXPECT_EQ(task3.done(), true);

    auto [id1, id2, id3] = result;
    EXPECT_NE(id1, id2);
    EXPECT_NE(id2, id3);
    EXPECT_NE(id1, id3);
}

}  // namespace

}  // namespace clice::testing

