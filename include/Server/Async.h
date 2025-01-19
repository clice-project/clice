#pragma once

#include <vector>
#include <chrono>
#include <cassert>
#include <cstdlib>
#include <utility>
#include <optional>
#include <concepts>
#include <coroutine>
#include <type_traits>

#include "Support/JSON.h"

#ifdef _WIN32
#define NOMINMAX
#endif

#include "uv.h"

#ifdef _WIN32
#undef THIS
#endif

#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/FunctionExtras.h"

namespace clice::async {

#define uv_check_call(func, ...)                                                                   \
    if(int error = func(__VA_ARGS__); error < 0) {                                                 \
        log::fatal("An error occurred in " #func ": {0}", uv_strerror(error));                     \
    }

template <typename T, typename U>
T& uv_cast(U* u) {
    assert(u && u->data && "uv_cast: invalid uv handle");
    return *static_cast<std::remove_cvref_t<T>*>(u->data);
}

using core_handle = std::coroutine_handle<>;

/// Schedule the coroutine to resume in the event loop.
void schedule(core_handle core);

template <typename T>
class Task;

namespace impl {

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

    auto final_suspend() noexcept {
        struct awaiter {
            core_handle waiting;

            bool await_ready() noexcept {
                return false;
            }

            void await_suspend(core_handle core) noexcept {
                if(waiting) {
                    /// In the final suspend point, this coroutine is already done.
                    /// So try to resume the waiting coroutine if it exists.
                    async::schedule(waiting);
                }
            }

            void await_resume() noexcept {}
        };

        return awaiter{waiting};
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
    Task(coroutine_handle handle) : core(handle) {}

    Task(const Task&) = delete;

    Task(Task&& other) : core(other.core) {
        other.core = nullptr;
    }

    Task& operator= (const Task&) = delete;

    Task& operator= (Task&& other) = delete;

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

void run();

template <typename Callback>
auto suspend(Callback&& callback) {
    struct suspend_awaiter {
        Callback callback;

        bool await_ready() noexcept {
            return false;
        }

        void await_suspend(core_handle handle) noexcept {
            callback(handle);
        }

        void await_resume() noexcept {}
    };

    return suspend_awaiter{std::forward<Callback>(callback)};
}

struct empty {};

template <typename Task, typename V = typename std::remove_cvref_t<Task>::value_type>
using task_value_t = std::conditional_t<std::is_void_v<V>, empty, V>;

template <typename... Tasks>
auto gather(Tasks&&... tasks) -> Task<std::tuple<task_value_t<Tasks>...>> {
    bool all_done = (tasks.done() && ...);

    if(!all_done) {
        llvm::SmallVector<core_handle, sizeof...(Tasks)> handles = {tasks.handle()...};
        bool started = false;

        if(!started) {
            for(auto handle: handles) {
                async::schedule(handle);
            }
            started = true;
        }

        while(!all_done) {
            co_await async::suspend([](core_handle handle) { async::schedule(handle); });
            all_done = (tasks.done() && ...);
        }
    }

    /// If all tasks are done, return the results.
    auto getResult = []<typename Task>(Task& task) {
        if constexpr(std::is_void_v<typename Task::value_type>) {
            return empty{};
        } else {
            return task.await_resume();
        }
    };
    co_return std::tuple{getResult(tasks)...};
}

/// Run the tasks in parallel and return the results.
template <typename... Tasks>
auto run(Tasks&&... tasks) {
    auto core = gather(std::forward<Tasks>(tasks)...);
    schedule(core.handle());
    async::run();
    assert(core.done() && "run: not done");
    return core.await_resume();
}

template <typename T>
    requires (!std::same_as<T, void>)
class Future {
public:
    bool await_ready() const noexcept {
        return value.has_value();
    }

    void await_suspend(core_handle waiting) noexcept {
        if(!value.has_value()) {
            waiters.push_back(waiting);
        } else {
            async::schedule(waiting);
        }
    }

    T await_resume() noexcept {
        assert(value.has_value() && "await_resume: value not set");
        return std::move(*value);
    }

    template <typename... Args>
    void emplace(Args&&... args) {
        value.emplace(std::forward<Args>(args)...);
        for(auto waiter: waiters) {
            async::schedule(waiter);
        }
    }

private:
    std::optional<T> value;
    std::vector<core_handle> waiters;
};

class Lock {
public:
    Lock(bool ready) : ready(ready) {}

    void lock() {
        ready = false;
    }

    void unlock() {
        ready = true;
    }

    Task<void> operator co_await() {
        while(!ready) {
            co_await suspend([](core_handle handle) { async::schedule(handle); });
        }
    }

private:
    bool ready = false;
};

namespace awaiter {

template <typename Ret>
struct thread_pool_base {
    std::optional<Ret> value;

    Ret await_resume() noexcept {
        assert(value.has_value() && "await_resume: value not set");
        return std::move(*value);
    }
};

template <>
struct thread_pool_base<void> {
    void await_resume() noexcept {}
};

template <typename Callback, typename Ret>
struct thread_pool : thread_pool_base<Ret> {
    uv_work_t work;
    Callback callback;
    core_handle waiting;

    bool await_ready() noexcept {
        return false;
    }

    void await_suspend(core_handle waiting) noexcept {
        work.data = this;
        this->waiting = waiting;

        /// This callback is called in the thread pool.
        auto work_cb = [](uv_work_t* work) {
            auto& awaiter = *static_cast<thread_pool*>(work->data);
            if constexpr(!std::is_void_v<Ret>) {
                awaiter.value.emplace(awaiter.callback());
            } else {
                awaiter.callback();
            }
        };

        /// This callback is called in the event loop thread.
        auto after_work_cb = [](uv_work_t* work, int status) {
            auto& awaiter = *static_cast<thread_pool*>(work->data);
            async::schedule(awaiter.waiting);
        };

        uv_queue_work(uv_default_loop(), &work, work_cb, after_work_cb);
    }
};

}  // namespace awaiter

/// Submit a task to the thread pool.
template <std::invocable<> Callback, typename R = std::invoke_result_t<Callback>>
auto submit(Callback&& callback) {
    using C = std::remove_cvref_t<Callback>;
    return awaiter::thread_pool<C, R>{{}, {}, std::forward<Callback>(callback)};
}

namespace awaiter {

struct sleep {
    uv_timer_t timer;
    std::chrono::milliseconds duration;
    core_handle waiting;

    bool await_ready() const noexcept {
        return duration.count() <= 0;
    }

    void await_suspend(core_handle waiting) noexcept {
        timer.data = this;
        this->waiting = waiting;

        auto callback = [](uv_timer_t* timer) {
            auto& awaiter = *static_cast<sleep*>(timer->data);
            async::schedule(awaiter.waiting);
            uv_close(reinterpret_cast<uv_handle_t*>(timer), nullptr);
        };

        uv_timer_init(uv_default_loop(), &timer);
        uv_timer_start(&timer, callback, duration.count(), 0);
    }

    void await_resume() noexcept {}
};

}  // namespace awaiter

/// Suspend the current coroutine for a duration.
inline auto wait_for(std::chrono::milliseconds duration) {
    return awaiter::sleep{{}, duration, {}};
}

struct Stats {
    using time_point = std::chrono::time_point<std::chrono::system_clock>;
    time_point mtime;
};

namespace awaiter {

struct stat {
    uv_fs_t fs;
    std::string path;
    Stats stats;
    core_handle waiting;

    bool await_ready() const noexcept {
        return false;
    }

    void await_suspend(core_handle waiting) noexcept {
        fs.data = this;
        this->waiting = waiting;

        auto callback = [](uv_fs_t* fs) {
            auto transform = [](uv_timespec_t& mtime) {
                using namespace std::chrono;
                return system_clock::time_point(duration_cast<system_clock::duration>(
                    seconds(mtime.tv_sec) + nanoseconds(mtime.tv_nsec)));
            };

            auto& awaiter = *static_cast<stat*>(fs->data);

            /// FIXME: handle error.
            awaiter.stats.mtime = transform(fs->statbuf.st_mtim);

            async::schedule(awaiter.waiting);
        };

        uv_fs_stat(uv_default_loop(), &fs, path.c_str(), callback);
    }

    Stats await_resume() noexcept {
        return stats;
    }
};

struct write {
    uv_fs_t fs;
    std::string path;
    char* data;
    size_t size;
    core_handle waiting;

    bool await_ready() const noexcept {
        return false;
    }

    void await_suspend(core_handle waiting) noexcept {
        fs.data = this;
        this->waiting = waiting;

        auto callback = [](uv_fs_t* fs) {
            auto& awaiter = *static_cast<write*>(fs->data);
            async::schedule(awaiter.waiting);

            uv_fs_t close_req;
            uv_fs_close(fs->loop, &close_req, fs->result, nullptr);
            uv_fs_req_cleanup(fs);
        };

        uv_fs_open(uv_default_loop(),
                   &fs,
                   path.c_str(),
                   O_WRONLY | O_CREAT | O_TRUNC,
                   0666,
                   nullptr);

        uv_buf_t buf[1] = {uv_buf_init(data, size)};
        uv_fs_write(uv_default_loop(), &fs, fs.result, buf, 1, 0, callback);
    }

    void await_resume() noexcept {}
};

}  // namespace awaiter

/// Get the file status asynchronously.
inline auto stat(llvm::StringRef path) {
    return awaiter::stat{{}, path.str(), {}, {}};
}

/// Write the data to the file asynchronously.
inline auto write(llvm::StringRef path, char* data, size_t size) {
    return awaiter::write{{}, path.str(), data, size, {}};
}

using Callback = llvm::unique_function<Task<void>(json::Value)>;

/// Listen on stdin/stdout, callback is called when there is a LSP message available.
void listen(Callback callback);

/// Listen on the given ip and port, callback is called when there is a LSP message available.
void listen(Callback callback, const char* ip, unsigned int port);

/// Spawn a new process and listen on its stdin/stdout.
void spawn(Callback callback, llvm::StringRef path, llvm::ArrayRef<std::string> args);

/// Write a JSON value to the client.
Task<> write(json::Value value);

}  // namespace clice::async
