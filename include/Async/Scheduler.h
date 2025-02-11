#pragma once

#include <chrono>
#include <tuple>

#include "libuv.h"
#include "Task.h"

namespace clice::async {

void run();

namespace awaiter {

template <typename Callback>
struct suspend {
    Callback callback;

    bool await_ready() noexcept {
        return false;
    }

    template <typename Promise>
    void await_suspend(std::coroutine_handle<Promise> handle) noexcept {
        callback(&handle.promise());
    }

    void await_resume() noexcept {}
};

}  // namespace awaiter

template <typename Callback>
auto suspend(Callback&& callback) {
    return awaiter::suspend<std::remove_cvref_t<Callback>>{std::forward<Callback>(callback)};
}

struct none {};

template <typename Task, typename V = typename std::remove_cvref_t<Task>::value_type>
using task_value_t = std::conditional_t<std::is_void_v<V>, none, V>;

template <typename... Tasks>
auto gather [[gnu::noinline]] (Tasks&&... tasks) -> Task<std::tuple<task_value_t<Tasks>...>> {
    /// FIXME: If remove noinline, the program crashes. Figure out in the future.
    (tasks.schedule(), ...);

    while(!(tasks.done() && ...)) {
        co_await async::suspend([](auto handle) { handle->schedule(); });
    }

    /// If all tasks are done, return the results.
    auto getResult = []<typename Task>(Task& task) {
        if constexpr(std::is_void_v<typename Task::value_type>) {
            return none{};
        } else {
            return task.result();
        }
    };
    co_return std::tuple{getResult(tasks)...};
}

/// Run the tasks in parallel and return the results.
template <typename... Tasks>
auto run(Tasks&&... tasks) {
    auto core = gather(std::forward<Tasks>(tasks)...);
    core.schedule();
    async::run();
    assert(core.done() && "run: not done");
    return core.result();
}

};  // namespace clice::async

