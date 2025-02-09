#include "Test/Test.h"
#include "Async/Async.h"

namespace clice::testing {

namespace {

TEST(Async, Cancel) {
    int x = 1;

    auto my_task = [&]() -> async::Task<> {
        x = 2;
        co_await async::sleep(std::chrono::milliseconds(1000));
        x = 3;
    };

    auto main = [&]() -> async::Task<> {
        auto task = my_task();
        task.schedule();
        co_await async::sleep(std::chrono::milliseconds(500));
        task.cancel();
        task.dispose();
    };

    auto p = main();
    p.schedule();

    async::run();

    EXPECT_EQ(x, 2);
}

}  // namespace

}  // namespace clice::testing
