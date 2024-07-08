#pragma once

#include <uv.h>
#include <optional>
#include <coroutine>

namespace clice {

/// An async coroutine that runs a callback on a worker thread.
template <typename Callback>
class async {
private:
    Callback callback;
    std::optional<decltype(callback())> result;
    uv_work_t req;
    std::coroutine_handle<> handle;

public:
    template <typename Fn>
        requires std::is_invocable_v<Fn>
    async(Fn&& fn) : callback(std::forward<Fn>(fn)) {}

    bool await_ready(this const async& self) noexcept { return false; }

    void await_suspend(this async& self, std::coroutine_handle<> handle) noexcept {
        self.handle = handle;
        self.req.data = &self;
        uv_queue_work(
            uv_default_loop(),
            &self.req,
            [](uv_work_t* req) {
                auto& self = *static_cast<async*>(req->data);
                self.result = self.callback();
            },
            [](uv_work_t* req, int status) {
                if(status != 0) {
                    // TODO: handle error
                }
                auto& self = *static_cast<async*>(req->data);
                self->handle.resume();
            });
    }

    decltype(auto) await_resume(this async& self) noexcept {
        assert(self.result.has_value());
        return *self.result;
    }
};

template <typename T>
async(T) -> async<std::decay_t<T>>;

template <typename T>
class Task {
public:
    struct promise_type {
        std::optional<T> value;
        std::coroutine_handle<> continuation;

        Task get_return_object(this promise_type& self) {
            return Task{std::coroutine_handle<promise_type>::from_promise(self)};
        }

        std::suspend_always initial_suspend() { return {}; }

        std::suspend_always final_suspend() noexcept {
            if(continuation) {
                continuation.resume();
            }
            return {};
        }

        void return_value(T&& val) { value = std::move(val); }

        void return_value(const T& val) { value = val; }

        void unhandled_exception() { std::terminate(); }
    };

    Task(std::coroutine_handle<promise_type> handle) : handle(handle) {}

    ~Task() {
        if(handle && !handle.done()) {
            handle.destroy();
        }
    }

    T get() { return std::move(*handle.promise().value); }

    bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> awaiting) noexcept {
        handle.promise().continuation = awaiting;
        handle.resume();
    }

    T await_resume() { return std::move(*handle.promise().value); }

private:
    std::coroutine_handle<promise_type> handle;
};

template <>
class Task<void> {
public:
    struct promise_type {
        std::coroutine_handle<> continuation;

        Task get_return_object(this promise_type& self) {
            return Task{std::coroutine_handle<promise_type>::from_promise(self)};
        }

        std::suspend_always initial_suspend() { return {}; }

        std::suspend_always final_suspend() noexcept {
            if(continuation) {
                continuation.resume();
            }
            return {};
        }

        void return_void() {}

        void unhandled_exception() { std::terminate(); }
    };

    Task(std::coroutine_handle<promise_type> handle) : handle(handle) {}

    ~Task() {
        if(handle && !handle.done()) {
            handle.destroy();
        }
    }

    bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> awaiting) noexcept {
        handle.promise().continuation = awaiting;
        handle.resume();
    }

    void await_resume() {}

private:
    std::coroutine_handle<promise_type> handle;
};

}  // namespace clice
