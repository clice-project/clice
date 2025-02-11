#include "Test/Test.h"
#include "Async/Async.h"
#include "Async/ThreadPool.h"

namespace clice::testing {

namespace {

TEST(Async, TaskAwait) {
    static auto my_task1 = []() -> async::Task<int> {
        co_return 1;
    };

    static auto my_task2 = []() -> async::Task<int> {
        auto result = co_await my_task1();
        co_return result + 1;
    };

    static auto my_task3 = []() -> async::Task<int> {
        auto result = co_await my_task2();
        co_return result + 1;
    };

    /// auto [result] = async::run(my_task3());
    /// EXPECT_EQ(result, 3);
}

TEST(Async, TaskDispose) {
    // static int x = 1;
    //
    // struct X {
    //    ~X() {
    //        x += 1;
    //    }
    //};
    //
    // auto my_task = [&]() -> async::Task<> {
    //    X x;
    //    co_await async::sleep(std::chrono::milliseconds(300));
    //};
    //
    // auto task = my_task();
    // task.schedule();
    // task.dispose();
    //
    // async::run();
    //
    // EXPECT_EQ(x, 2);
    //
    // auto main = [&]() -> async::Task<> {
    //    auto task = my_task();
    //    task.schedule();
    //    co_await async::sleep(std::chrono::milliseconds(100));
    //    task.cancel();
    //    task.dispose();
    //};
    //
    // auto p = main();
    // p.schedule();
    //
    // async::run();
    //
    // EXPECT_EQ(x, 3);
}

TEST(Async, TaskCancel) {
    // int x = 1;
    //
    // auto my_task = [&]() -> async::Task<> {
    //    x = 2;
    //    co_await async::sleep(std::chrono::milliseconds(300));
    //    x = 3;
    //};
    //
    // auto main = [&]() -> async::Task<> {
    //    auto task = my_task();
    //    task.schedule();
    //    co_await async::sleep(std::chrono::milliseconds(100));
    //    task.cancel();
    //    task.dispose();
    //};
    //
    // auto p = main();
    // p.schedule();
    //
    // async::run();
    //
    // EXPECT_EQ(x, 2);
}

}  // namespace

}  // namespace clice::testing
