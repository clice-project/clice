#include "Test/Test.h"
#include "Async/Async.h"

namespace clice::testing {

namespace {

TEST(Async, Sleep) {
    int x = 1;
    auto task_gen = [&]() -> async::Task<> {
        x = 2;
        co_await async::sleep(100);
        x = 3;
    };

    auto task = task_gen();
    async::run(task);

    EXPECT_EQ(x, 3);
}

}  // namespace

}  // namespace clice::testing
