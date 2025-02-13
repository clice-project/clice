#include "Async/FileSystem.h"

namespace clice::async::awaiter {}

namespace clice::async::fs {

namespace awaiter {

template <typename Derived, typename Ret = void>
struct fs : async::awaiter::uv<fs<Derived, Ret>, uv_fs_t, Ret> {
    int start(auto callback) {
        return static_cast<Derived*>(this)->start(callback);
    }

    void cleanup() {
        uv_fs_req_cleanup(&this->request);
    }

    auto result() {
        return static_cast<Derived*>(this)->result();
    }
};

struct open : fs<open, handle> {
    const char* path;
    int flags;

    int start(uv_fs_cb cb) {
        /// `uv_fs_open` will copy the path, so we don't need to worry about the
        /// lifetime of the path.
        return uv_fs_open(async::loop, &request, path, flags, 0666, cb);
    }

    auto result() {
        return request.result;
    }
};

struct close : fs<close> {
    handle file;

    int start(uv_fs_cb cb) {
        return uv_fs_close(async::loop, &request, file, cb);
    }
};

struct read : fs<read, ssize_t> {
    handle file;
    uv_buf_t bufs[1];

    int start(uv_fs_cb cb) {
        return uv_fs_read(async::loop, &request, file, bufs, 1, -1, cb);
    }

    auto result() {
        return request.result;
    }
};

struct write : fs<write> {
    handle file;
    uv_buf_t bufs[1];

    int start(uv_fs_cb cb) {
        return uv_fs_write(async::loop, &request, file, bufs, 1, 0, cb);
    }
};

struct stat : fs<stat, Stats> {
    const char* path;

    int start(uv_fs_cb cb) {
        return uv_fs_stat(async::loop, &request, path, cb);
    }

    auto result() {
        Stats stats;
        stats.mtime = std::chrono::milliseconds(request.statbuf.st_mtim.tv_sec * 1000);
        return stats;
    }
};

}  // namespace awaiter

static int transformFlags(Mode mode) {
    int flags = 0;

    if(mode & Mode::Read) {
        flags |= O_RDONLY;
    }

    if(mode & Mode::Write) {
        flags |= O_WRONLY;
    }

    if(mode & Mode::ReadWrite) {
        flags |= O_RDWR;
    }

    if(mode & Mode::Create) {
        flags |= O_CREAT;
    }

    if(mode & Mode::Append) {
        flags |= O_APPEND;
    }

    if(mode & Mode::Truncate) {
        flags |= O_TRUNC;
    }

    if(mode & Mode::Exclusive) {
        flags |= O_EXCL;
    }

    return flags;
}

Result<handle> open(std::string path, Mode mode) {
    co_return co_await awaiter::open{
        .path = path.c_str(),
        .flags = transformFlags(mode),
    };
}

Result<void> close(handle file) {
    co_return co_await awaiter::close{.file = file};
}

Result<ssize_t> read(handle file, char* buffer, std::size_t size) {
    co_return co_await awaiter::read{
        .file = file,
        .bufs = {uv_buf_init(buffer, size)},
    };
}

Result<std::string> read(std::string path, Mode mode) {
    /// Open the file.
    auto file = co_await open(path, mode);
    if(!file) {
        co_return std::unexpected(file.error());
    }

    /// Read the file content.
    std::string content;

    char buffer[4096];
    while(true) {
        auto result = co_await read(*file, buffer, sizeof(buffer));
        if(!result) {
            co_return std::unexpected(result.error());
        }

        if(*result == 0) {
            break;
        }

        content.append(buffer, *result);
    }

    /// Close the file.
    if(auto result = co_await close(*file); !result) {
        co_return std::unexpected(result.error());
    }

    co_return content;
}

Result<void> write(handle file, char* buffer, std::size_t size) {
    co_return co_await awaiter::write{
        .file = file,
        .bufs = {uv_buf_init(buffer, size)},
    };
}

Result<void> write(std::string path, char* buffer, std::size_t size, Mode mode) {
    auto file = co_await open(path, mode);
    if(!file) {
        co_return std::unexpected(file.error());
    }

    if(auto result = co_await write(*file, buffer, size); !result) {
        co_return std::unexpected(result.error());
    }

    if(auto result = co_await close(*file); !result) {
        co_return std::unexpected(result.error());
    }

    co_return std::expected<void, std::error_code>();
}

Result<Stats> stat(std::string path) {
    co_return co_await awaiter::stat{.path = path.c_str()};
}

}  // namespace clice::async::fs

