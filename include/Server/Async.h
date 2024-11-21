#pragma once

#include "uv.h"

#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/FunctionExtras.h"
#include "llvm/Support/raw_ostream.h"

#include "Support/JSON.h"

#include <new>
#include <chrono>
#include <thread>
#include <utility>
#include <cstdlib>
#include <cassert>
#include <concepts>
#include <coroutine>
#include <type_traits>

namespace clice {

template <typename T>
struct promise;

}

namespace clice::async {

template <typename Value>
struct Result {
    union {
        Value value;
    };

    Result() {}

    ~Result() {}

    bool await_ready() noexcept {
        return false;
    }

    decltype(auto) await_resume() noexcept {
        return value;
    }

    template <typename T>
    void return_value(T&& value) noexcept {
        new (&value) Value(std::forward<T>(value));
    }
};

template <>
struct Result<void> {
    bool await_ready() noexcept {
        return false;
    }

    void await_resume() noexcept {}

    void return_void() noexcept {}
};

template <typename T, typename U>
T& uv_cast(U* u) {
    assert(u->data && "uv_cast: invalid uv handle");
    return *static_cast<T*>(u->data);
}

inline uv_loop_t* loop = uv_default_loop();

/// Schedule a coroutine to run in the event loop.
inline void schedule(std::coroutine_handle<> handle) {
    assert(handle && "schedule: invalid coroutine handle");
    uv_async_t* async = new uv_async_t();
    async->data = handle.address();
    uv_async_init(loop, async, [](uv_async_t* async) {
        auto handle = std::coroutine_handle<>::from_address(async->data);
        handle.resume();
        uv_close((uv_handle_t*)async, [](uv_handle_t* handle) { delete handle; });
    });
    uv_async_send(async);
}

template <typename T>
void schedule(promise<T> promise) {
    schedule(promise.handle());
}

/// Schedule a task to run in the thread pool.
template <std::invocable Task>
auto schedule_task(Task&& task) {
    using Func = std::remove_cvref_t<Task>;
    using Ret = std::invoke_result_t<Func>;

    static_assert(!std::is_reference_v<Ret>, "return type must not be a reference");

    struct Awaiter : Result<Ret> {
        Func func;
        std::coroutine_handle<> caller;

        void await_suspend(std::coroutine_handle<> caller) noexcept {
            this->caller = caller;
            uv_work_t* work = new uv_work_t{.data = this};
            uv_queue_work(
                loop,
                work,
                [](uv_work_t* work) {
                    auto& awaiter = uv_cast<Awaiter>(work);
                    if constexpr(!std::is_void_v<Ret>) {
                        new (&awaiter.value) Ret(awaiter.func());
                    } else {
                        awaiter.func();
                    }
                },
                [](uv_work_t* work, int status) {
                    auto awaiter = uv_cast<Awaiter>(work);
                    awaiter.caller.resume();
                    delete work;
                });
        }
    };

    return Awaiter{{}, std::forward<Task>(task)};
}

inline auto sleep(std::chrono::milliseconds ms) {
    struct Awaiter {
        std::chrono::milliseconds ms;
        std::coroutine_handle<> caller;

        bool await_ready() noexcept {
            return ms.count() == 0;
        }

        void await_suspend(std::coroutine_handle<> caller) noexcept {
            this->caller = caller;
            uv_timer_t* timer = new uv_timer_t{.data = this};
            uv_timer_init(loop, timer);
            uv_timer_start(
                timer,
                [](uv_timer_t* timer) {
                    auto& awaiter = uv_cast<Awaiter>(timer);
                    awaiter.caller.resume();
                    uv_close((uv_handle_t*)timer, [](uv_handle_t* handle) { delete handle; });
                },
                ms.count(),
                0);
        }

        void await_resume() noexcept {}
    };

    return Awaiter{ms};
}

struct FinalAwaiter {
    std::coroutine_handle<> caller;

    bool await_ready() noexcept {
        return false;
    }

    void await_suspend(std::coroutine_handle<> self) noexcept {
        self.destroy();
        /// If this coroutine is a top-level coroutine, its caller is empty.
        if(!caller) {
            return;
        }

        /// Schedule the caller to run in the event loop.
        schedule(caller);
    }

    void await_resume() noexcept {}
};

}  // namespace clice::async

namespace clice {

template <typename T>
class promise {
public:
    struct promise_type : async::Result<T> {
        std::coroutine_handle<> caller;

        auto get_return_object() {
            return promise{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        void unhandled_exception() {
            std::abort();
        }

        std::suspend_always initial_suspend() {
            return {};
        }

        auto final_suspend() noexcept {
            return async::FinalAwaiter{.caller = caller};
        }
    };

    using coroutine_handle = std::coroutine_handle<promise_type>;

    promise(coroutine_handle handle) : h(handle) {}

    bool await_ready() noexcept {
        return false;
    }

    void await_suspend(std::coroutine_handle<> handle) noexcept {
        this->h.promise().caller = handle;
        async::schedule(*this);
    }

    decltype(auto) await_resume() noexcept
        requires (!std::is_void_v<T>)
    {
        return std::move(h.promise().value);
    }

    void await_resume() noexcept
        requires (std::is_void_v<T>)
    {}

    coroutine_handle handle() const noexcept {
        return h;
    }

private:
    coroutine_handle h;
};

template <typename T = bool>
class Future {
public:
    bool await_ready() {
        return false;
    }

    void setReady() {
        isReady = true;
        if(isReady) {
            for(auto handle: handles) {
                handle.resume();
            }
            handles.clear();
        }
    }

    void await_suspend(std::coroutine_handle<> handle) {
        handles.emplace_back(handle);
        if(isReady) {
            for(auto handle: handles) {
                handle.resume();
            }
            handles.clear();
        }
    }

    void await_resume() {}

private:
    bool isReady = false;
    llvm::SmallVector<std::coroutine_handle<>, 6> handles;
};

template <typename... Ps>
int run(Ps&&... ps) {
    (schedule(std::forward<Ps>(ps)), ...);
    return uv_run(uv_default_loop(), UV_RUN_DEFAULT);
}

struct Writer {
    void write(json::Value id, json::Value result);

public:
    void* handle;
};

struct Server {
public:
    using Callback = llvm::unique_function<promise<void>(json::Value, Writer&)>;

    Server(Callback callback);

    Server(Callback callback, const char* ip, unsigned int port);

    void run() {
        uv_run(async::loop, UV_RUN_DEFAULT);
    }

public:
    Writer writer;
    Callback callback;
};

}  // namespace clice
