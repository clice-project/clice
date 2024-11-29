#include "../Test.h"
#include "Server/Async.h"

namespace {

using namespace clice;

async::promise<int> add(int a, int b) {
    co_return a + b;
}

async::promise<int> add2(int a, int b) {
    auto result = co_await add(a, b);
    co_return result;
}

TEST(clice, coroutine) {
    auto p = add2(1, 2);
    async::run(p);
    ASSERT_TRUE(p.done());
    ASSERT_EQ(p.handle().promise().value, 3);
    p.destroy();
}

}  // namespace
