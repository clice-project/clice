#include "Test/Test.h"
#include "Async/Event.h"
#include "Async/Scheduler.h"

namespace clice::testing {

namespace {

TEST(Async, Event) {
    async::Event event;

    int x = 0;

    auto task_func1 = [&]() -> async::Task<> {
        EXPECT_EQ(x, 0);
        co_await event;
        EXPECT_EQ(x, 1);
        x = 2;
    };

    auto task_func2 = [&]() -> async::Task<> {
        EXPECT_EQ(x, 0);
        co_await event;
        EXPECT_EQ(x, 2);
        x = 3;
    };

    auto main_func = [&]() -> async::Task<> {
        x = 1;
        event.set();

        co_return;
    };

    auto task1 = task_func1();
    task1.schedule();

    auto task2 = task_func2();
    task2.schedule();

    auto main = main_func();
    main.schedule();

    async::run();
}

}  // namespace

}  // namespace clice::testing
