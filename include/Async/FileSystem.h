#pragma once

#include <chrono>

#include "UV.h"
#include "Coroutine.h"

#include "Support/JSON.h"

#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/FunctionExtras.h"

namespace clice::async {

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
    std::chrono::milliseconds mtime;
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
                return milliseconds(duration_cast<milliseconds>(seconds(mtime.tv_sec) +
                                                                nanoseconds(mtime.tv_nsec)));
            };

            auto& awaiter = *static_cast<stat*>(fs->data);

            /// FIXME: handle error.
            awaiter.stats.mtime = transform(fs->statbuf.st_mtim);

            async::schedule(awaiter.waiting);

            uv_fs_t close_req;
            uv_fs_close(fs->loop, &close_req, fs->result, nullptr);
            uv_fs_req_cleanup(fs);
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

struct read {
    uv_fs_t fs;
    std::string path;
    core_handle waiting;

    bool await_ready() const noexcept {
        return false;
    }

    void await_suspend(core_handle waiting) noexcept {
        fs.data = this;
        this->waiting = waiting;

        auto callback = [](uv_fs_t* fs) {
            auto& awaiter = *static_cast<read*>(fs->data);
            async::schedule(awaiter.waiting);

            uv_fs_t close_req;
            uv_fs_close(fs->loop, &close_req, fs->result, nullptr);
            uv_fs_req_cleanup(fs);
        };

        uv_fs_open(uv_default_loop(), &fs, path.c_str(), O_RDONLY, 0, nullptr);

        uv_buf_t buf[1] = {};
        uv_fs_read(uv_default_loop(), &fs, fs.result, buf, 1, 0, callback);
    }
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
