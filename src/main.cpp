#include <LSP/Server.h>
#include <coroutine>
#include <thread>
#include <iostream>

uv_loop_t* loop = nullptr;

struct Task {
    struct promise_type {
        Task get_return_object() { return Task{Handle::from_promise(*this)}; }

        std::suspend_never initial_suspend() { return {}; }

        std::suspend_always final_suspend() noexcept { return {}; }

        void return_void() {}

        void unhandled_exception() { std::terminate(); }
    };

    struct Handle {
        std::coroutine_handle<promise_type> coro;

        static Handle from_promise(promise_type& p) {
            return Handle{std::coroutine_handle<promise_type>::from_promise(p)};
        }

        ~Handle() {
            if(coro)
                coro.destroy();
        }
    };

    Handle h;
};

struct uv_awaitable {
    uv_work_t req;
    std::function<void()> work_fn;
    std::coroutine_handle<> handle;

    bool await_ready() { return false; }

    void await_suspend(std::coroutine_handle<> handle) {
        this->handle = handle;
        req.data = this;
        std::cout << "Scheduling work..." << std::endl;
        uv_queue_work(
            loop,
            &req,
            [](uv_work_t* req) {
                auto* self = static_cast<uv_awaitable*>(req->data);
                self->work_fn();
            },
            [](uv_work_t* req, int status) {
                auto& handle = static_cast<uv_awaitable*>(req->data)->handle;
                if(handle.done()) {
                    std::cout << "Work done" << std::endl;
                } else {
                    handle.resume();
                }
            });
    }

    auto await_resume() { return 1; }
};

Task async_factorial(int n) {
    long result = 1;
    auto s = co_await uv_awaitable{.work_fn = [&result, n] {
        for(int i = 1; i <= n; ++i) {
            result *= i;
        }
    }};
    std::cout << "Factorial of " << n << " is " << result << std::endl;
}

int main() {
    auto& server = clice::server;
    server.initialize();
    return 0;
}
