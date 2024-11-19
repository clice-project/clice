#include "uv.h"
#include "Server/Config.h"
#include "Server/Server.h"
#include "Basic/Location.h"
#include "llvm/Support/raw_ostream.h"

namespace clice {

namespace {

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
            pos = str.end() - buffer.begin();
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

}  // namespace

void Server::run(llvm::unique_function<void()> callback) {
    this->callback = std::move(callback);
    uv_run(uv_default_loop(), UV_RUN_DEFAULT);
}

Server::Server(const config::ServerOption& option) {

    static uv_loop_t* loop = uv_default_loop();
    static uv_idle_t idle;

    /// start the idle loop.
    uv_idle_init(loop, &idle);
    idle.data = this;
    uv_idle_start(&idle, [](uv_idle_t* handle) {
        auto& server = *static_cast<Server*>(handle->data);
        server.callback();
    });

    static auto on_alloc = +[](uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
        static llvm::SmallString<4096> buffer;
        buffer.resize_for_overwrite(suggested_size);
        buf->base = buffer.data();
        buf->len = suggested_size;
    };

    static auto on_read = [](uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
        if(nread > 0) {
            static MessageBuffer messageBuffer;
            messageBuffer.append({buf->base, static_cast<std::size_t>(nread)});
            if(auto message = messageBuffer.peek(); !message.empty()) {
                auto& server = *static_cast<Server*>(stream->data);
                if(auto json = json::parse(message)) {
                    server.messages.emplace_back(*json);
                    messageBuffer.consume();
                } else {
                    llvm::errs() << "JSON PARSE ERROR " << json.takeError() << "\n";
                }
            }
        } else if(nread < 0) {
            if(nread != UV_EOF) {
                fprintf(stderr, "Error reading from stdin: %s\n", uv_strerror(nread));
            }
            uv_close((uv_handle_t*)stream, NULL);
        }
    };

    /// initialize the socket or pipe.
    if(option.mode == "socket") {
        static uv_tcp_t server;
        static uv_tcp_t client;

        uv_tcp_init(loop, &server);
        uv_tcp_init(loop, &client);
        server.data = this;
        client.data = this;

        struct sockaddr_in addr;
        /// FIXME:
        uv_ip4_addr(option.address.c_str(), option.port, &addr);

        uv_tcp_bind(&server, (const struct sockaddr*)&addr, 0);
        int r = uv_listen((uv_stream_t*)&server, 128, [](uv_stream_t* server, int status) {
            if(status < 0) {
                llvm::errs() << "New connection error\n";
                return;
            }

            if(uv_accept(server, (uv_stream_t*)&client) == 0) {
                llvm::errs() << "Client connected.\n";
                uv_read_start((uv_stream_t*)&client, on_alloc, on_read);
            } else {
                uv_close((uv_handle_t*)&client, NULL);
            }
        });

        if(r) {
            llvm::errs() << "Listen error: " << uv_strerror(r) << "\n";
        }

        writer = (uv_stream_t*)&client;
    } else if(option.mode == "pipe") {
        static uv_pipe_t stdin_pipe;
        static uv_pipe_t stdout_pipe;

        uv_pipe_init(loop, &stdin_pipe, 0);
        uv_pipe_init(loop, &stdout_pipe, 0);
        stdin_pipe.data = this;
        stdout_pipe.data = this;

        uv_pipe_open(&stdin_pipe, 0);
        uv_pipe_open(&stdout_pipe, 1);

        uv_read_start((uv_stream_t*)&stdin_pipe, on_alloc, on_read);

        writer = (uv_stream_t*)&stdout_pipe;
    } else {
        llvm::errs() << "Unknown mode: " << option.mode << "\n";
    }
}

void Server::write(llvm::StringRef message) {
    uv_write_t* req = new uv_write_t();
    uv_buf_t buf = uv_buf_init(const_cast<char*>(message.data()), message.size());

    auto on_write = [](uv_write_t* req, int status) {
        if(status < 0) {
            llvm::errs() << "Write error: " << uv_strerror(status) << "\n";
        }
        delete req;
    };

    uv_write(req, static_cast<uv_stream_t*>(writer), &buf, 1, on_write);
}

void Server::request() {}

void Server::response(json::Value id, json::Value result) {
    json::Value response = json::Object{
        {"jsonrpc", "2.0" },
        {"id",      id    },
        {"result",  result},
    };

    struct Buffer {
        llvm::SmallString<128> header;
        llvm::SmallString<4096> message;
    };

    Buffer* buffer = new Buffer();

    llvm::raw_svector_ostream os(buffer->message);
    os << response;

    llvm::raw_svector_ostream sos(buffer->header);
    sos << "Content-Length: " << buffer->message.size() << "\r\n\r\n";

    uv_buf_t bufs[2] = {
        uv_buf_init(buffer->header.data(), buffer->header.size()),
        uv_buf_init(buffer->message.data(), buffer->message.size()),
    };

    uv_write_t* req = new uv_write_t();
    req->data = buffer;

    auto on_write = [](uv_write_t* req, int status) {
        if(status < 0) {
            llvm::errs() << "Write error: " << uv_strerror(status) << "\n";
        }

        delete static_cast<Buffer*>(req->data);
        delete req;
    };

    int r = uv_write(req, static_cast<uv_stream_t*>(writer), bufs, 2, on_write);

    if(r) {
        llvm::errs() << "Write error: " << uv_strerror(r) << "\n";
    }
}

void Server::notify() {}

void Server::error() {}

}  // namespace clice
