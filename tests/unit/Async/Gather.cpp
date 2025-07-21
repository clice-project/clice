#include "Test/Test.h"
#include "Async/Async.h"

namespace clice::testing {

namespace {

TEST(Async, GatherPack) {
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

TEST(Async, GatherRange) {
    std::vector<int> args;
    for(int i = 0; i < 30; ++i) {
        args.push_back(i);
    }

    std::vector<int> results;

    auto task_gen = [&](int x) -> async::Task<bool> {
        co_await async::sleep(10);
        results.push_back(x);
        co_return true;
    };

    auto core = async::gather(args, task_gen);
    async::run(core);

    EXPECT_EQ(args, results);
    EXPECT_EQ(core.result(), true);
}

TEST(Async, GatherCancel) {
    std::vector<int> args;
    for(int i = 0; i < 30; ++i) {
        args.push_back(i);
    }

    std::vector<int> results;

    auto task_gen = [&](int x) -> async::Task<bool> {
        co_await async::sleep(10);
        results.push_back(x);
        co_return false;
    };

    auto core = async::gather(args, task_gen);
    async::run(core);

    EXPECT_EQ(results.size(), 1);
    EXPECT_EQ(core.result(), false);
}

}  // namespace

}  // namespace clice::testing
