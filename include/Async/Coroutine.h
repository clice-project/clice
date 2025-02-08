#pragma once

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <coroutine>
#include <source_location>

#include "Support/Format.h"
#include "llvm/ADT/PointerIntPair.h"

namespace clice::async {

template <typename T>
class Task;

using core_handle = std::coroutine_handle<>;

void schedule(struct promise_handle* core);

struct promise_handle {
    enum class State : uint8_t {
        /// The task is waiting to be scheduled.
        Pending,

        /// The task is running.
        Running,

        /// The task is finished.
        Finished,

        /// The task is cancelled.
        Cancelled,
    };

    llvm::PointerIntPair<void*, 2, State> data;

    /// The coroutine handle that is waiting for the task to complete.
    /// If this is a top-level coroutine, it is empty.
    promise_handle* continuation = nullptr;

    template <typename Promise>
    void set(std::coroutine_handle<Promise> handle) {
        data.setInt(State::Pending);
        data.setPointer(handle.address());
    }

    core_handle handle() const noexcept {
        return core_handle::from_address(data.getPointer());
    }

    bool done() {
        return handle().done();
    }

    void resume() {
        handle().resume();
    }

    void destroy() {
        handle().destroy();
    }
};

namespace awaiter {

struct final {
    promise_handle* continuation;

    bool await_ready() noexcept {
        return false;
    }

    void await_suspend(core_handle) noexcept {
        if(continuation) {
            /// In the final suspend point, this coroutine is already done.
            /// So try to resume the waiting coroutine if it exists.
            async::schedule(continuation);
        }
    }

    void await_resume() noexcept {}
};

}  // namespace awaiter

template <typename T = void>
class Task {
public:
    template <typename V>
    struct promise_result {
        std::optional<V> value;

        template <typename U>
        void return_value(U&& val) noexcept {
            assert(!value.has_value() && "return_value: value already set");
            value.emplace(std::forward<U>(val));
        }
    };

    template <>
    struct promise_result<void> {
        void return_void() noexcept {}
    };

    struct promise_type : promise_handle, promise_result<T> {
        auto get_return_object() {
            /// Get the coroutine handle from the promise.
            auto handle = std::coroutine_handle<promise_type>::from_promise(*this);
            /// Set the handle to the promise.
            this->set(handle);
            return Task<T>(handle);
        }

        void unhandled_exception() {
            std::abort();
        }

        auto initial_suspend() {
            return std::suspend_always();
        }

        awaiter::final final_suspend() noexcept {
            return awaiter::final{continuation};
        }

        auto handle() {
            return std::coroutine_handle<promise_type>::from_promise(*this);
        }
    };

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
    template <typename Promise>
    void await_suspend(std::coroutine_handle<Promise> waiting) noexcept {
        /// When another coroutine awaits this task, set the waiting coroutine.
        assert(!core.promise().continuation && "await_suspend: already waiting");
        core.promise().continuation = &waiting.promise();

        /// Schedule the task to run. Note that the waiting coroutine is scheduled
        /// in final_suspend. See `impl::promise_type::final_suspend`.
        async::schedule(&core.promise());
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
