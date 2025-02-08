#pragma once

#include <tuple>

#include "libuv.h"
#include "Coroutine.h"

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
    (async::schedule(&tasks.handle().promise()), ...);

    while(!(tasks.done() && ...)) {
        co_await async::suspend([](auto handle) { async::schedule(handle); });
    }

    /// If all tasks are done, return the results.
    auto getResult = []<typename Task>(Task& task) {
        if constexpr(std::is_void_v<typename Task::value_type>) {
            return none{};
        } else {
            return task.await_resume();
        }
    };
    co_return std::tuple{getResult(tasks)...};
}

/// Run the tasks in parallel and return the results.
template <typename... Tasks>
auto run(Tasks&&... tasks) {
    auto core = gather(std::forward<Tasks>(tasks)...);
    schedule(&core.handle().promise());
    async::run();
    assert(core.done() && "run: not done");
    return core.await_resume();
}

namespace impl::awaiter {

template <typename Ret>
struct thread_pool_base {
    std::optional<Ret> value;

    Ret await_resume() noexcept {
        assert(value.has_value() && "await_resume: value not set");
        return std::move(*value);
    }
};

template <>
struct thread_pool_base<void> {
    void await_resume() noexcept {}
};

template <typename Function, typename Ret>
struct thread_pool : thread_pool_base<Ret> {
    /// The libuv work request.
    uv_work_t request;

    /// The function to run in the thread pool.
    Function function;

    /// The coroutine handle waiting for the result.
    promise_handle* waiting;

    bool await_ready() noexcept {
        return false;
    }

    template <typename Promise>
    void await_suspend(std::coroutine_handle<Promise> waiting) noexcept {
        request.data = this;
        this->waiting = &waiting.promise();

        auto work_cb = [](uv_work_t* work) {
            auto& awaiter = *static_cast<thread_pool*>(work->data);
            if constexpr(!std::is_void_v<Ret>) {
                awaiter.value.emplace(awaiter.function());
            } else {
                awaiter.function();
            }
        };

        auto after_work_cb = [](uv_work_t* work, int status) {
            auto& awaiter = *static_cast<thread_pool*>(work->data);
            async::schedule(awaiter.waiting);
        };

        uv_queue_work(uv_default_loop(), &request, work_cb, after_work_cb);
    }
};

}  // namespace impl::awaiter

template <std::invocable<> Callback, typename R = std::invoke_result_t<Callback>>
auto submit(Callback&& callback) {
    using C = std::remove_cvref_t<Callback>;
    return impl::awaiter::thread_pool<C, R>{{}, {}, std::forward<Callback>(callback)};
}

class Lock {
public:
    Lock(bool& locked) : locked(locked) {}

    Task<void> operator co_await() {
        while(locked) {
            co_await async::suspend([](auto handle) { async::schedule(handle); });
        }
        locked = true;
    }

    void unlock() {
        locked = false;
    }

    ~Lock() {
        locked = false;
    }

private:
    bool& locked;
};

}  // namespace clice::async
