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
    static int x = 1;

    struct X {
        ~X() {
            x += 1;
        }
    };

    auto my_task = [&]() -> async::Task<> {
        X x;
        co_await async::sleep(300);
    };

    auto task = my_task();
    task.schedule();
    task.dispose();
    async::run();

    EXPECT_EQ(x, 2);

    auto main = [&]() -> async::Task<> {
        auto task = my_task();
        task.schedule();
        co_await async::sleep(100);
        task.cancel();
        task.dispose();
    };

    async::run(main());

    EXPECT_EQ(x, 3);
}

TEST(Async, TaskCancel) {
    int x = 1;

    auto task1 = [&]() -> async::Task<> {
        x = 2;
        co_await async::sleep(300);
        x = 3;
    };

    auto main = [&]() -> async::Task<> {
        auto task = task1();
        task.schedule();
        co_await async::sleep(100);
        task.cancel();
        task.dispose();
    };

    async::run(main());

    EXPECT_EQ(x, 2);
}

TEST(Async, TaskCancelRecursively) {
    int x = 0;
    int y = 0;
    int z = 0;

    auto task1 = [&]() -> async::Task<> {
        x = 1;
        co_await async::sleep(300);
        println("Task1 done");
        x = 2;
    };

    auto task2 = [&]() -> async::Task<> {
        auto task = task1();
        y = 1;
        co_await task;
        y = 2;
    };

    auto task3 = [&]() -> async::Task<> {
        auto task = task2();
        z = 1;
        co_await task;
        z = 2;
    };

    auto main = [&]() -> async::Task<> {
        auto task = task3();
        task.schedule();
        co_await async::sleep(100);
        task.cancel();
        task.dispose();
    };

    async::run(main());

    /// All tasks should be cancelled recursively.
    EXPECT_EQ(x, 1);
    EXPECT_EQ(y, 1);
    EXPECT_EQ(z, 1);
}

}  // namespace

}  // namespace clice::testing
