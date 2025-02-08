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

    std::source_location location;

    template <typename Promise>
    void set(std::coroutine_handle<Promise> handle) {
        data.setInt(State::Pending);
        data.setPointer(handle.address());
    }

    auto handle() const noexcept {
        return std::coroutine_handle<>::from_address(data.getPointer());
    }

    void schedule();

    bool done() const noexcept {
        return handle().done();
    }

    void resume() {
        auto state = data.getInt();
        switch(data.getInt()) {
            case State::Pending:
            case State::Running: {
                data.setInt(State::Running);
                handle().resume();
                break;
            }

            case State::Finished: {
                assert(false && "resume: already finished");
                break;
            }

            case State::Cancelled: {
                break;
            }
        }
    }

    void destroy() {
        handle().destroy();
    }

    void cancel() {
        data.setInt(State::Cancelled);
    }
};

namespace awaiter {

/// The awaiter for the final suspend point of `Task`.
struct final {
    promise_handle* continuation;

    bool await_ready() noexcept {
        return false;
    }

    template <typename Promise>
    auto await_suspend(std::coroutine_handle<Promise> current) noexcept {
        /// In the final suspend point, this coroutine is already done.
        /// So try to resume the waiting coroutine if it exists.
        if(continuation) {
            continuation->schedule();
        }
    }

    void await_resume() noexcept {}
};

/// We want `Task` to be awaitable.
template <typename T, typename P>
struct task {
    std::coroutine_handle<P> handle;

    bool await_ready() noexcept {
        return false;
    }

    template <typename Promise>
    auto await_suspend(std::coroutine_handle<Promise> waiting) noexcept {
        /// If this `Task` is awaited from another coroutine, we should schedule
        /// the this task first.
        handle.promise().schedule();

        /// Store the waiting coroutine in the promise for later scheduling.
        /// It will be scheduled in the final suspend point.
        assert(!handle.promise().continuation && "await_suspend: already waiting");
        handle.promise().continuation = &waiting.promise();
    }

    T await_resume() noexcept {
        if constexpr(!std::is_void_v<T>) {
            assert(handle.promise().value.has_value() && "await_resume: value not set");
            return std::move(*handle.promise().value);
        }
    }
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
        promise_type(std::source_location location = std::source_location::current()) {
            set(handle());
            this->location = location;
        };

        auto get_return_object() {
            return Task<T>(handle());
        }

        auto initial_suspend() {
            return std::suspend_always();
        }

        auto final_suspend() noexcept {
            return awaiter::final{continuation};
        }

        void unhandled_exception() {
            std::abort();
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

    void schedule() {
        core.promise().schedule();
    }

    void stacktrace() {
        promise_handle* handle = core;
        while(handle) {
            println("{}:{}:{}",
                    handle->location.file_name(),
                    handle->location.line(),
                    handle->location.function_name());
            handle = handle->continuation;
        }
    }

    void cancel() {
        core.promise().cancel();
    }

    auto operator co_await() const noexcept {
        return awaiter::task<T, promise_type>{core};
    }

    T result() {
        if constexpr(!std::is_void_v<T>) {
            return std::move(core.promise().value.value());
        }
    }

private:
    coroutine_handle core;
};

}  // namespace clice::async
