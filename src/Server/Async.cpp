#include "Server/Async.h"

#include "llvm/ADT/SmallString.h"

namespace clice::async {

using Callback = llvm::unique_function<promise<void>(json::Value)>;
uv_loop_t* loop = uv_default_loop();

namespace {

Callback callback = {};
uv_stream_t* writer = {};

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
                async::schedule(callback(std::move(*json)));
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

void start_server(Callback callback) {
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
    uv_check_call(uv_run, async::loop, UV_RUN_DEFAULT);
}

void start_server(Callback callback, const char* ip, unsigned int port) {
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
    uv_check_call(uv_run, async::loop, UV_RUN_DEFAULT);
}

void write(json::Value id, json::Value result) {
    json::Value response = json::Object{
        {"jsonrpc", "2.0" },
        {"id",      id    },
        {"result",  result},
    };

    struct Buffer {
        uv_write_t req;
        llvm::SmallString<128> header;
        llvm::SmallString<4096> message;
    };

    Buffer* buffer = new Buffer();
    buffer->req.data = buffer;

    llvm::raw_svector_ostream os(buffer->message);
    os << response;

    llvm::raw_svector_ostream sos(buffer->header);
    sos << "Content-Length: " << buffer->message.size() << "\r\n\r\n";

    uv_buf_t bufs[2] = {
        uv_buf_init(buffer->header.data(), buffer->header.size()),
        uv_buf_init(buffer->message.data(), buffer->message.size()),
    };

    auto on_write = [](uv_write_t* req, int status) {
        if(status < 0) {
            log::fatal("An error occurred while writing: {0}", uv_strerror(status));
        }

        delete static_cast<Buffer*>(req->data);
    };

    uv_check_call(uv_write, &buffer->req, writer, bufs, 2, on_write);
}

}  // namespace clice::async
