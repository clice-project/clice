#include <deque>

#include "Server/Async.h"
#include "Server/Logger.h"

namespace clice::async {

namespace {

/// The default event loop.
uv_loop_t* loop = uv_default_loop();

/// The task queue waiting for resuming.
std::deque<std::coroutine_handle<>> tasks;

Callback callback = {};

uv_stream_t* writer = {};

/// Whether the server is listening.
bool listened = false;

}  // namespace

/// This function is called by the event loop to resume the tasks.
static void event_loop(uv_idle_t* handle) {
    if(tasks.empty()) {
        return;
    }

    auto task = tasks.front();
    tasks.pop_front();
    task.resume();

    if(tasks.empty() && !listened) {
        uv_stop(loop);
    }
}

void schedule(std::coroutine_handle<> core) {
    assert(core && !core.done() && "schedule: invalid coroutine handle");
    tasks.emplace_back(core);
}

void run() {
    uv_idle_t idle;
    uv_idle_init(loop, &idle);
    uv_idle_start(&idle, event_loop);

    uv_run(loop, UV_RUN_DEFAULT);
}

namespace {

void on_alloc(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
    /// This function is called synchronously before `on_read`. See the implementation of
    /// `uv__read` in libuv/src/unix/stream.c. So it is safe to use a static buffer here.
    static llvm::SmallString<65536> buffer;
    buffer.resize_for_overwrite(suggested_size);
    buf->base = buffer.data();
    buf->len = suggested_size;
}

class MessageBuffer {
public:
    MessageBuffer() = default;

    void append(llvm::StringRef message) {
        buffer += message;
    }

    llvm::StringRef peek() {
        llvm::StringRef str = buffer;
        std::size_t length = 0;
        if(str.consume_front("Content-Length: ") && !str.consumeInteger(10, length) &&
           str.consume_front("\r\n\r\n") && str.size() >= length) {
            auto result = str.substr(0, length);
            pos = result.end() - buffer.begin();
            return result;
        }
        return {};
    }

    void consume() {
        buffer.erase(buffer.begin(), buffer.begin() + pos);
        pos = 0;
    }

private:
    std::size_t pos;
    llvm::SmallString<4096> buffer;
};

void on_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
    /// We have at most one connection and use default event loop. So there is no data race
    /// risk. It is safe to use a static buffer here.

    /// FIXME: use a more efficient data structure.
    static MessageBuffer buffer;
    if(nread > 0) {
        buffer.append({buf->base, static_cast<std::size_t>(nread)});
        if(auto message = buffer.peek(); !message.empty()) {
            if(auto json = json::parse(message)) {
                /// This is a top-level coroutine.
                auto core = callback(std::move(*json));
                /// It will be destroyed in final suspend point.
                /// So we release it here.
                async::schedule(core.release());
                buffer.consume();
            } else {
                log::fatal("An error occurred while parsing JSON: {0}", json.takeError());
            }
        }
    } else if(nread < 0) {
        if(nread != UV_EOF) {
            log::fatal("An error occurred while reading: {0}", uv_strerror(nread));
        }
        uv_close((uv_handle_t*)stream, NULL);
    }
}

}  // namespace

void listen(Callback callback) {
    static uv_pipe_t in;
    static uv_pipe_t out;

    async::callback = std::move(callback);
    writer = reinterpret_cast<uv_stream_t*>(&out);

    uv_check_call(uv_pipe_init, async::loop, &in, 0);
    uv_check_call(uv_pipe_init, async::loop, &out, 0);

    uv_check_call(uv_pipe_open, &in, 0);
    uv_check_call(uv_pipe_open, &out, 1);

    uv_check_call(uv_read_start, (uv_stream_t*)&in, async::on_alloc, async::on_read);

    log::info("Server started in pipe mode");
    async::listened = true;
}

void listen(Callback callback, const char* ip, unsigned int port) {
    static uv_tcp_t server;
    static uv_tcp_t client;

    async::callback = std::move(callback);
    writer = reinterpret_cast<uv_stream_t*>(&client);

    uv_check_call(uv_tcp_init, async::loop, &server);
    uv_check_call(uv_tcp_init, async::loop, &client);

    struct ::sockaddr_in addr;
    uv_check_call(uv_ip4_addr, ip, port, &addr);
    uv_check_call(uv_tcp_bind, &server, (const struct sockaddr*)&addr, 0);

    auto on_connection = [](uv_stream_t* server, int status) {
        if(status < 0) {
            log::fatal("An error occurred while listening: {0}", uv_strerror(status));
        }

        uv_check_call(uv_accept, server, (uv_stream_t*)&client);
        log::info("New connection accepted");
        uv_check_call(uv_read_start, (uv_stream_t*)&client, async::on_alloc, async::on_read);
    };

    uv_check_call(uv_listen, (uv_stream_t*)&server, 128, on_connection);

    log::info("Server started in socket mode at {0}:{1}", ip, port);
    async::listened = true;
}

void spawn(Callback callback, llvm::StringRef path, llvm::ArrayRef<std::string> args) {
    static uv_pipe_t in;
    static uv_pipe_t out;
    static uv_pipe_t err;

    async::callback = std::move(callback);
    writer = reinterpret_cast<uv_stream_t*>(&in);

    uv_check_call(uv_pipe_init, async::loop, &in, 0);
    uv_check_call(uv_pipe_init, async::loop, &out, 0);
    uv_check_call(uv_pipe_init, async::loop, &err, 0);

    static uv_process_t process;
    static uv_process_options_t options;

    static uv_stdio_container_t stdio[3];
    stdio[0].flags = static_cast<uv_stdio_flags>(UV_CREATE_PIPE | UV_READABLE_PIPE);
    stdio[0].data.stream = (uv_stream_t*)&in;

    stdio[1].flags = static_cast<uv_stdio_flags>(UV_CREATE_PIPE | UV_WRITABLE_PIPE);
    stdio[1].data.stream = (uv_stream_t*)&out;

    stdio[2].flags = static_cast<uv_stdio_flags>(UV_CREATE_PIPE | UV_WRITABLE_PIPE);
    stdio[2].data.stream = (uv_stream_t*)&err;

    options = {[](uv_process_t* req, int64_t exit_status, int term_signal) {
        printf("Child process exited with status %ld, signal %d\n", exit_status, term_signal);
        uv_close((uv_handle_t*)req, NULL);
    }};
    options.stdio = stdio;
    options.stdio_count = 3;

    static llvm::SmallString<128> file = path;
    options.file = file.c_str();

    static llvm::SmallString<1024> buffer;
    static llvm::SmallVector<char*> argv;
    std::size_t size = 0;
    size += path.size() + 1;
    for(auto& arg: args) {
        size += arg.size() + 1;
    }
    buffer.resize_for_overwrite(size);
    argv.push_back(buffer.end());
    buffer.append(path);
    buffer.push_back('\0');
    for(auto& arg: args) {
        argv.push_back(buffer.end());
        buffer.append(arg);
        buffer.push_back('\0');
    }
    options.args = argv.data();

    uv_check_call(uv_spawn, async::loop, &process, &options);
    uv_check_call(uv_read_start, (uv_stream_t*)&out, async::on_alloc, async::on_read);
    uv_check_call(
        uv_read_start,
        (uv_stream_t*)&err,
        async::on_alloc,
        [](uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
            if(nread > 0) {
                log::warn("{0}", llvm::StringRef{buf->base, static_cast<std::size_t>(nread)});
            } else if(nread < 0) {
                if(nread != UV_EOF) {
                    log::fatal("An error occurred while reading: {0}", uv_strerror(nread));
                }
                uv_close((uv_handle_t*)stream, NULL);
            }
        });

    log::info("Process spawned: {0}", path);
    async::listened = true;
}

/// Write a JSON value to the client.
Task<> write(json::Value value) {
    struct awaiter {
        uv_write_t write;
        uv_buf_t buf[2];
        llvm::SmallString<128> header;
        llvm::SmallString<4096> message;
        core_handle waiting;

        bool await_ready() const noexcept {
            return false;
        }

        void await_suspend(core_handle waiting) noexcept {
            write.data = this;

            this->waiting = waiting;
            buf[0] = uv_buf_init(header.data(), header.size());
            buf[1] = uv_buf_init(message.data(), message.size());

            uv_check_call(uv_write, &write, writer, buf, 2, [](uv_write_t* req, int status) {
                if(status < 0) {
                    log::fatal("An error occurred while writing: {0}", uv_strerror(status));
                }

                auto& awaiter = uv_cast<struct awaiter>(req);
                async::schedule(awaiter.waiting);
            });
        }

        void await_resume() noexcept {}
    } awaiter;

    llvm::raw_svector_ostream(awaiter.message) << value;
    llvm::raw_svector_ostream(awaiter.header)
        << "Content-Length: " << awaiter.message.size() << "\r\n\r\n";

    co_await awaiter;
}

}  // namespace clice::async
