#include "llvm/ADT/SmallVector.h"
#include <iostream>
#include <optional>
#include <source_location>
#include <utility>
#include <uv.h>
#include <coroutine>

namespace clice {

uv_loop_t* loop = uv_default_loop();

template <typename Func>
struct awaiter {

    awaiter(Func&& func) : func(std::forward<Func>(func)) {}

    bool await_ready() noexcept {
        return false;
    }

    void await_suspend(std::coroutine_handle<> h) noexcept {
        request.data = this;
        uv_queue_work(
            loop,
            &request,
            [](uv_work_t* req) {
                auto* self = static_cast<awaiter*>(req->data);
                self->result = self->func();
            },
            [](uv_work_t* req, int status) {
                auto* self = static_cast<awaiter*>(req->data);
                self->coro.resume();
            });
    }

    decltype(auto) await_resume() {
        return std::move(*result);
    }

    Func func;
    uv_work_t request;
    std::coroutine_handle<> coro;
    std::optional<decltype(func())> result;
};

template <typename R>
struct Result {
    template <typename T>
    void return_value(T&& value) noexcept {}
};

template <>
struct Result<void> {
    void return_void() noexcept {}
};

template <typename R = void>
struct Task {
    struct promise_type;

    using handle = std::coroutine_handle<promise_type>;

    struct promise_type : Result<R> {
        auto get_return_object() noexcept {
            return Task{handle::from_promise(*this)};
        }

        std::suspend_always initial_suspend() noexcept {
            return {};
        }

        auto final_suspend() noexcept {
            struct final_awaiter {
                std::coroutine_handle<> caller;

                bool await_ready() noexcept {
                    return false;
                }

                void await_suspend(std::coroutine_handle<> h) noexcept {
                    h.destroy();
                    caller.resume();
                }

                void await_resume() noexcept {}
            };

            return final_awaiter{caller};
        }

        void unhandled_exception() noexcept {
            std::terminate();
        }

        std::coroutine_handle<> caller;
    };

    bool await_ready() noexcept {
        return false;
    }

    void await_suspend(std::coroutine_handle<> caller) noexcept {
        h.promise().caller = caller;
        h.resume();
    }

    decltype(auto) await_resume() {}

    handle h;
};

template <typename Func>
auto run(Func&& func) {
    return awaiter{std::forward<Func>(func)};
}

Task<> test() {
    co_await run([] {
        printf("Hello from thread pool.\n");
        return 1;
    });
    printf("Thread pool work completed.\n");
}

Task<> test2() {
    co_await test();
}

}  // namespace clice

