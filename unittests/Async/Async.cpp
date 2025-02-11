#include "Test/Test.h"
#include "Async/ThreadPool.h"
#include <thread>

namespace clice::testing {

namespace {

TEST(Async, Submit) {
    using result = async::Task<std::expected<std::thread::id, std::error_code>>;

    auto task = []() -> result {
        co_return co_await async::submit([]() {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            return std::this_thread::get_id();
        });
    }();

    auto task2 = []() -> result {
        co_return co_await async::submit([]() {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            return std::this_thread::get_id();
        });
    }();

    auto task3 = []() -> result {
        co_return co_await async::submit([]() {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            return std::this_thread::get_id();
        });
    }();
}

}  // namespace

}  // namespace clice::testing

