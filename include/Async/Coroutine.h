#pragma once

#include <cassert>
#include <cstdlib>
#include <optional>
#include <coroutine>
#include <source_location>

namespace clice::async {

template <typename T>
class Task;

using core_handle = std::coroutine_handle<>;

/// Schedule the coroutine to resume in the event loop.
void schedule(core_handle core);

namespace impl {

namespace awaiter {

struct final {
    core_handle waiting;

    bool await_ready() noexcept {
        return false;
    }

    void await_suspend(core_handle) noexcept {
        if(waiting) {
            /// In the final suspend point, this coroutine is already done.
            /// So try to resume the waiting coroutine if it exists.
            async::schedule(waiting);
        }
    }

    void await_resume() noexcept {}
};

}  // namespace awaiter

template <typename T>
struct promise_base {
    std::optional<T> value;

    template <typename U>
    void return_value(U&& val) noexcept {
        assert(!value.has_value() && "return_value: value already set");
        value.emplace(std::forward<U>(val));
    }
};

template <>
struct promise_base<void> {
    void return_void() noexcept {}
};

template <typename T>
struct promise_type : promise_base<T> {
    /// The coroutine handle that is waiting for the task to complete.
    /// If this is a top-level coroutine, it is empty.
    core_handle waiting;

    auto get_return_object() {
        return Task<T>(std::coroutine_handle<promise_type>::from_promise(*this));
    }

    void unhandled_exception() {
        std::abort();
    }

    auto initial_suspend() {
        return std::suspend_always();
    }

    awaiter::final final_suspend() noexcept {
        return awaiter::final{waiting};
    }
};

}  // namespace impl

template <typename T = void>
class Task {
public:
    using promise_type = impl::promise_type<T>;

    using coroutine_handle = std::coroutine_handle<promise_type>;

    using value_type = T;

public:
    Task() = default;

    Task(coroutine_handle handle) : core(handle) {}

    Task(const Task&) = delete;

    Task(Task&& other) noexcept : core(other.core) {
        other.core = nullptr;
    }

    Task& operator= (const Task&) = delete;

    Task& operator= (Task&& other) noexcept {
        if(core) {
            core.destroy();
        }
        core = other.core;
        other.core = nullptr;
        return *this;
    }

    ~Task() {
        if(core) {
            core.destroy();
        }
    }

public:
    coroutine_handle handle() const noexcept {
        return core;
    }

    coroutine_handle release() noexcept {
        auto handle = core;
        core = nullptr;
        return handle;
    }

    bool empty() const noexcept {
        return !core;
    }

    bool done() const noexcept {
        return core.done();
    }

    bool await_ready() const noexcept {
        return false;
    }

    /// Task is also awaitable.
    void await_suspend(core_handle waiting) noexcept {
        /// When another coroutine awaits this task, set the waiting coroutine.
        assert(!core.promise().waiting && "await_suspend: already waiting");
        core.promise().waiting = waiting;

        /// Schedule the task to run. Note that the waiting coroutine is scheduled
        /// in final_suspend. See `impl::promise_type::final_suspend`.
        async::schedule(core);
    }

    T await_resume() noexcept {
        if constexpr(!std::is_void_v<T>) {
            assert(core.promise().value.has_value() && "await_resume: value not set");
            return std::move(*core.promise().value);
        }
    }

private:
    coroutine_handle core;
};

}  // namespace clice::async
