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
                self.handle.resume();
            });
    }

    decltype(auto) await_resume(this async& self) noexcept {
        assert(self.result.has_value());
        return std::move(*self.result);
    }
};

template <typename T>
async(T) -> async<std::decay_t<T>>;

template <typename T>
class Task {
public:
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;

    struct promise_type {
        std::optional<T> value;
        std::coroutine_handle<> caller;

        Task get_return_object() { return Task(handle_type::from_promise(*this)); }

        std::suspend_always initial_suspend() { return {}; }

        std::suspend_always final_suspend() noexcept {
            if(caller) {
                caller.resume();
            }
            return {};
        }

        void return_value(T&& val) { value = std::move(val); }

        void return_value(const T& val) { value = val; }

        void unhandled_exception() { std::terminate(); }
    };

    Task(handle_type handle) : handle(handle) {}

    ~Task() {
        if(handle && !handle.done()) {
            handle.destroy();
        }
    }

    void resume() { handle.resume(); }

    bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> caller) noexcept {
        handle.promise().caller = caller;
        handle.resume();
    }

    T await_resume() { return std::move(*handle.promise().value); }

private:
    handle_type handle;
};

template <>
class Task<void> {
public:
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;

    struct promise_type {
        std::coroutine_handle<> caller;

        Task get_return_object() { return Task(handle_type::from_promise(*this)); }

        std::suspend_always initial_suspend() { return {}; }

        std::suspend_always final_suspend() noexcept {
            if(caller) {
                caller.resume();
            }
            return {};
        }

        void return_void() {}

        void unhandled_exception() { std::terminate(); }
    };

    Task(handle_type handle) : handle(handle) {}

    //~Task() {
    //    if(handle && !handle.done()) {
    //        handle.destroy();
    //    }
    //}

    void resume() { handle.resume(); }

    bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> caller) noexcept {
        handle.promise().caller = caller;
        handle.resume();
    }

    void await_resume() {}

private:
    handle_type handle;
};

}  // namespace clice
