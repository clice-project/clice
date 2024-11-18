#include <uv.h>
#include <thread>
#include <utility>
#include <coroutine>
#include <source_location>

namespace clice {}  // namespace clice

namespace clice::asyncio {

/// Unique loop instance.
inline uv_loop_t* loop = uv_default_loop();

template <typename Value>
struct result {
    union {
        Value value;
    };

    result() = default;

    result(const result&) = delete;

    result& operator= (const result&) = delete;

    bool await_ready() noexcept {
        return false;
    }

    decltype(auto) await_resume() {
        return std::move(value);
    }
};

template <std::invocable Task>
auto schedule(Task&& task) {
    using R = std::invoke_result_t<Task>;

    struct awaiter : result<R> {
        Task task;
        uv_work_t request;
        std::coroutine_handle<> h;

        void await_suspend(std::coroutine_handle<> caller) noexcept {
            h = caller;
            request.data = this;
            uv_queue_work(
                loop,
                &request,
                [](uv_work_t* req) {
                    auto* self = static_cast<awaiter*>(req->data);
                    new (&self->value) R(self->task());
                },
                [](uv_work_t* req, int status) {
                    auto* self = static_cast<awaiter*>(req->data);
                    /// FIXME: resolve status and handle errors.
                    if(!self->h.done()) {
                        self->h.resume();
                    }
                });
        }
    };

    return awaiter{{}, std::forward<Task>(task)};
}

auto sleep(std::size_t milliseconds) {
    struct awaiter {
        uv_timer_t timer;
        std::size_t milliseconds;
        std::coroutine_handle<> h;

        bool await_ready() noexcept {
            return false;
        }

        void await_suspend(std::coroutine_handle<> caller) noexcept {
            h = caller;
            timer.data = this;
            uv_timer_init(loop, &timer);
            uv_timer_start(
                &timer,
                [](uv_timer_t* handle) {
                    auto* self = static_cast<awaiter*>(handle->data);
                    if(!self->h.done()) {
                        self->h.resume();
                    }
                    uv_close(reinterpret_cast<uv_handle_t*>(handle), nullptr);
                },
                milliseconds,
                0);
        }

        void await_resume() noexcept {}
    };

    return awaiter{{}, milliseconds};
}

struct final_awaiter {
    std::coroutine_handle<> caller;

    bool await_ready() noexcept {
        return false;
    }

    void await_suspend(std::coroutine_handle<> h) noexcept {
        h.destroy();
        if(caller && !caller.done()) {
            caller.resume();
        }
    }

    void await_resume() noexcept {}
};

template <typename T>
class promise {
public:
    struct promise_type {
        union {
            T value;
        };

        std::coroutine_handle<> caller;

        auto get_return_object() noexcept {
            return handle::from_promise(*this);
        };

        std::suspend_always initial_suspend() noexcept {
            return {};
        }

        auto final_suspend() noexcept {
            return final_awaiter{caller};
        }

        void unhandled_exception() noexcept {
            std::terminate();
        }

        template <typename Value>
        void return_value(Value&& value) noexcept {
            new (&this->value) Value(std::forward<Value>(value));
        }
    };

    using handle = std::coroutine_handle<promise_type>;

    promise(handle h) : h(h) {}

    bool await_ready() noexcept {
        return false;
    }

    void await_suspend(std::coroutine_handle<> caller) noexcept {
        h.promise().caller = caller;
        if(!h.done()) {
            h.resume();
        }
    }

    decltype(auto) await_resume() {
        return std::move(h.promise().value);
    }

    void resume() {
        if(!h.done()) {
            h.resume();
        }
    }

private:
    handle h;
};

template <>
class promise<void> {
public:
    struct promise_type {
        std::coroutine_handle<> caller;

        auto get_return_object() noexcept {
            return handle::from_promise(*this);
        };

        std::suspend_always initial_suspend() noexcept {
            return {};
        }

        auto final_suspend() noexcept {
            return final_awaiter{caller};
        }

        void unhandled_exception() noexcept {
            std::terminate();
        }

        void return_void() noexcept {}
    };

    using handle = std::coroutine_handle<promise_type>;

    promise(handle h) : h(h) {}

    bool await_ready() noexcept {
        return false;
    }

    void await_suspend(std::coroutine_handle<> caller) noexcept {
        h.promise().caller = caller;
        if(!h.done()) {
            h.resume();
        }
    }

    void await_resume() {}

private:
    handle h;
};

template <typename T, typename Handle>
T& uv_cast(Handle handle) {
    return *reinterpret_cast<T*>(handle->data);
}

}  // namespace clice::asyncio

using namespace clice;
using namespace clice::asyncio;

promise<int> test() {
    co_await asyncio::sleep(1000);
    int result = co_await asyncio::schedule([]() -> int {
        printf("Hello from async task!\n");
        return 42;
    });
    co_return result;
}

