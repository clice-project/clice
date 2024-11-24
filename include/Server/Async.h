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

namespace clice::async {

template <typename T>
struct promise;

extern uv_loop_t* loop;

template <typename T, typename U>
T& uv_cast(U* u) {
    assert(u && u->data && "uv_cast: invalid uv handle");
    return *static_cast<T*>(u->data);
}

using Callback = llvm::unique_function<promise<void>(json::Value)>;

void start_server(Callback callback);

void start_server(Callback callback, const char* ip, unsigned int port);

void write(json::Value id, json::Value result);

template <typename Value>
struct result {
    union {
        Value value;
    };

    result() {}

    ~result() {}

    bool await_ready() noexcept {
        return false;
    }

    decltype(auto) await_resume() noexcept {
        return std::move(value);
    }

    template <typename T>
    void return_value(T&& val) noexcept {
        new (&value) Value(std::forward<T>(val));
    }
};

template <>
struct result<void> {
    bool await_ready() noexcept {
        return false;
    }

    void await_resume() noexcept {}

    void return_void() noexcept {}
};

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

template <typename Task, typename Ret = std::invoke_result_t<Task>>
struct task_awaiter : result<Ret> {
    std::remove_cvref_t<Task> task;
    std::coroutine_handle<> caller;

    void await_suspend(std::coroutine_handle<> caller) noexcept {
        static_assert(!std::is_reference_v<Ret>, "return type must not be a reference");
        this->caller = caller;
        uv_work_t* work = new uv_work_t{.data = this};
        uv_queue_work(
            loop,
            work,
            [](uv_work_t* work) {
                auto& awaiter = uv_cast<task_awaiter>(work);
                if constexpr(!std::is_void_v<Ret>) {
                    new (&awaiter.value) Ret(awaiter.task());
                } else {
                    awaiter.task();
                }
            },
            [](uv_work_t* work, int status) {
                auto& awaiter = uv_cast<task_awaiter>(work);
                awaiter.caller.resume();
                delete work;
            });
    }
};

/// Schedule a task to run in the thread pool.
template <std::invocable Task>
task_awaiter<Task> schedule_task(Task&& task) {
    return {{}, std::forward<Task>(task)};
}

struct sleep_awaiter {
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
                auto& awaiter = uv_cast<sleep_awaiter>(timer);
                awaiter.caller.resume();
                uv_close((uv_handle_t*)timer, [](uv_handle_t* handle) { delete handle; });
            },
            ms.count(),
            0);
    }

    void await_resume() noexcept {}
};

inline sleep_awaiter sleep(std::chrono::milliseconds ms) {
    return {ms};
}

struct fs_awaiter {
    std::string path;
    std::coroutine_handle<> caller;
    std::chrono::system_clock::time_point modified;

    bool await_ready() noexcept {
        return false;
    }

    void await_suspend(std::coroutine_handle<> caller) noexcept {
        this->caller = caller;
        uv_fs_t* req = new uv_fs_t{.data = this};
        uv_fs_open(loop, req, path.c_str(), UV_FS_O_RDONLY, 0, [](uv_fs_t* req) {
            auto& awaiter = uv_cast<fs_awaiter>(req);
            if(req->result < 0) {
                llvm::errs() << "Error: " << uv_strerror(req->result) << "\n";
                awaiter.caller.resume();
                delete req;
                return;
            }

            uv_fs_close(loop, req, req->result, [](uv_fs_t* req) {
                auto& awaiter = uv_cast<fs_awaiter>(req);
                uv_timespec_t& mtime = req->statbuf.st_mtim;
                using namespace std::chrono;
                awaiter.modified =
                    system_clock::time_point(seconds(mtime.tv_sec) + nanoseconds(mtime.tv_nsec));
                awaiter.caller.resume();
                delete req;
            });
        });
    }

    decltype(auto) await_resume() noexcept {
        return modified;
    }
};

inline fs_awaiter modified_time(llvm::StringRef path) {
    return {path.str()};
}

template <typename... Ps>
int run(Ps&&... ps) {
    (schedule(std::forward<Ps>(ps)), ...);
    return uv_run(loop, UV_RUN_DEFAULT);
}

template <typename T>
struct awaiter {
    using coroutine_handle = typename promise<T>::coroutine_handle;

    coroutine_handle h;

    bool await_ready() noexcept {
        return false;
    }

    void await_suspend(std::coroutine_handle<> handle) noexcept {
        h.promise().caller = handle;
        async::schedule(h);
    }

    decltype(auto) await_resume() noexcept
        requires (!std::is_void_v<T>)
    {
        return std::move(h.promise().value);
    }

    void await_resume() noexcept
        requires (std::is_void_v<T>)
    {}
};

template <typename T>
struct promise_type : result<T> {
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
                async::schedule(caller);
            }

            void await_resume() noexcept {}
        };

        return FinalAwaiter{.caller = caller};
    }
};

template <typename T>
class promise {
public:
    using promise_type = async::promise_type<T>;

    using coroutine_handle = std::coroutine_handle<promise_type>;

    promise(coroutine_handle handle) : h(handle) {}

    promise(const promise&) = delete;

    promise(promise&& other) : h(other.h) {
        other.h = nullptr;
    }

    awaiter<T> operator co_await() const noexcept {
        return {h};
    }

    coroutine_handle handle() const noexcept {
        return h;
    }

private:
    coroutine_handle h;
};

}  // namespace clice::async

