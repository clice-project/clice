#include "Test/Test.h"
#include "Async/Event.h"
#include "Async/Sleep.h"
#include "Async/Scheduler.h"

namespace clice::testing {

namespace {

TEST(Async, Gather) {
    int x = 0;

    auto task_gen = [&]() -> async::Task<int> {
        co_await async::sleep(100);
        x += 1;
        co_return x;
    };

    auto [a, b, c] = async::run(task_gen(), task_gen(), task_gen());

    EXPECT_EQ(a, 1);
    EXPECT_EQ(b, 2);
    EXPECT_EQ(c, 3);
}

}  // namespace

}  // namespace clice::testing
