#include "Test/Test.h"
#include "Async/Async.h"

namespace clice::testing {

namespace {

TEST(Async, Lock) {
    async::Lock lock;

    int x = 0;

    auto task1 = [&]() -> async::Task<> {
        auto guard = co_await lock.try_lock();
        co_await async::sleep(5);
        EXPECT_EQ(x, 0);
        co_await async::sleep(10);
        EXPECT_EQ(x, 0);
        co_await async::sleep(5);
        x = 1;
    };

    auto task2 = [&]() -> async::Task<> {
        auto guard = co_await lock.try_lock();
        co_await async::sleep(5);
        EXPECT_EQ(x, 1);
        co_await async::sleep(5);
        EXPECT_EQ(x, 1);
        co_await async::sleep(10);
        x = 2;
    };

    auto task3 = [&]() -> async::Task<> {
        auto guard = co_await lock.try_lock();
        co_await async::sleep(10);
        EXPECT_EQ(x, 2);
        co_await async::sleep(5);
        EXPECT_EQ(x, 2);
        co_await async::sleep(5);
    };

    async::run(task1(), task2(), task3());
}

TEST(Async, LockCancel) {
    async::Lock lock;

    int x = 0;
    int y = 0;

    auto task = [&]() -> async::Task<> {
        x += 1;
        auto guard = co_await lock.try_lock();
        co_await async::sleep(100);
        y += 1;
    };

    auto task1 = task();
    auto task2 = task();
    auto task3 = task();

    auto cancel = [&task2]() -> async::Task<> {
        co_await async::sleep(10);
        task2.cancel();
        task2.dispose();
    };

    task1.schedule();
    task2.schedule();
    task3.schedule();

    async::run(cancel());

    EXPECT_EQ(x, 3);
    EXPECT_EQ(y, 2);
}

}  // namespace

}  // namespace clice::testing
