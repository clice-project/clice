#include <spdlog/spdlog.h>
#include <Server/Transport .h>
#include <llvm/ADT/SmallVector.h>

namespace clice {

/// NOTE: Receiving and sending messages are only done in the main thread.
/// so it's safe to use the static variable as a buffer.

namespace {

class MessageBuffer {
    std::vector<char> buffer;
    std::size_t max = 0;

public:
    void write(std::string_view message) { buffer.insert(buffer.end(), message.begin(), message.end()); }

    std::string_view read() {
        std::string_view view = std::string_view(buffer.data(), buffer.size());
        auto start = view.find("Content-Length: ") + 16;
        auto end = view.find("\r\n\r\n");

        if(start != std::string_view::npos && end != std::string_view::npos) {
            std::size_t length = std::stoul(std::string(view.substr(start, end - start)));
            if(view.size() >= length + end + 4) {
                this->max = length + end + 4;
                return view.substr(end + 4, length);
            }
        }

        return {};
    }

    void clear() {
        if(max != 0) {
            buffer.erase(buffer.begin(), buffer.begin() + max);
            max = 0;
        }
    }
};

void alloc_buffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
    static llvm::SmallVector<char, 4096> buffer;
    buffer.resize(suggested_size);
    buf->base = buffer.data();
    buf->len = buffer.size();
}

// callback for reading data.
static void (*unique_callback)(std::string_view) = nullptr;

void read_callback(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
    static MessageBuffer buffer;
    if(nread > 0) {
        buffer.write(std::string_view(buf->base, nread));
        if(auto message = buffer.read(); !message.empty()) {
            unique_callback(message);
            buffer.clear();
        }
    } else if(nread < 0) {
        // if(nread != UV_EOF) {
        //     spdlog::error("Read error: {}", uv_err_name(nread));
        // }
        uv_close((uv_handle_t*)stream, NULL);
    }
}

}  // namespace

Pipe::Pipe(uv_loop_t* loop, void (*callback)(std::string_view)) {
    uv_pipe_init(loop, &stdin_pipe, 0);
    uv_pipe_init(loop, &stdout_pipe, 0);

    uv_pipe_open(&stdin_pipe, 0);
    uv_pipe_open(&stdout_pipe, 1);

    unique_callback = callback;
    uv_read_start(reinterpret_cast<uv_stream_t*>(&stdin_pipe), alloc_buffer, read_callback);
}

void Pipe::write(std::string_view message) {
    static uv_buf_t buf;
    static uv_write_t req;
    buf = uv_buf_init(const_cast<char*>(message.data()), message.size());

    auto state = uv_write(&req, reinterpret_cast<uv_stream_t*>(&stdout_pipe), &buf, 1, NULL);
    if(state < 0) {
        spdlog::error("Error writing to stdout: {}", uv_strerror(state));
    }
}

static uv_tcp_t unique_client;

Socket::Socket(uv_loop_t* loop, void (*callback)(std::string_view), const char* host, unsigned int port) {
    uv_tcp_init(loop, &server);
    sockaddr_in addr;
    uv_ip4_addr(host, port, &addr);
    uv_tcp_bind(&server, (const struct sockaddr*)&addr, 0);
    unique_callback = callback;
    uv_listen(reinterpret_cast<uv_stream_t*>(&server), 1, [](uv_stream_t* server, int status) {
        if(status < 0) {
            spdlog::error("Listen error: {}", uv_strerror(status));
            return;
        }

        spdlog::info("Server listening on port {}", 50505);
        uv_tcp_init(uv_default_loop(), (uv_tcp_t*)&unique_client);
        if(uv_accept(server, (uv_stream_t*)&unique_client) == 0) {
            uv_read_start((uv_stream_t*)&unique_client, alloc_buffer, read_callback);
        } else {
            uv_close((uv_handle_t*)&unique_client, NULL);
        }
    });
}

void Socket::write(std::string_view message) {
    static uv_buf_t buf;
    static uv_write_t req;
    buf = uv_buf_init(const_cast<char*>(message.data()), message.size());

    auto state = uv_write(&req, (uv_stream_t*)&unique_client, &buf, 1, NULL);
    if(state < 0) {
        spdlog::error("Error writing to socket: {}", uv_strerror(state));
    }
}

}  // namespace clice
