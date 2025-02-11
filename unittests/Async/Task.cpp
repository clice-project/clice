#include "Test/Test.h"
#include "Async/Async.h"
#include "Async/ThreadPool.h"

namespace clice::testing {

namespace {

TEST(Async, Run) {
    async::run();
}

TEST(Async, TaskSchedule) {
    auto task_gen = []() -> async::Task<int> {
        co_return 1;
    };

    auto task = task_gen();
    task.schedule();

    async::run();

    EXPECT_TRUE(task.done());
    EXPECT_EQ(task.result(), 1);
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
