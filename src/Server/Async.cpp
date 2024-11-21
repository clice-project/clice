#include "Server/Async.h"

#include "llvm/ADT/SmallString.h"

namespace clice::async {

namespace {

static void onAlloc(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
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

void onRead(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
    /// We have at most one connection and use default event loop. So there is no data race
    /// risk. It is safe to use a static buffer here.

    /// FIXME: use a more efficient data structure.
    static MessageBuffer messageBuffer;
    if(nread > 0) {
        messageBuffer.append({buf->base, static_cast<std::size_t>(nread)});
        if(auto message = messageBuffer.peek(); !message.empty()) {
            auto& server = *static_cast<Server*>(stream->data);
            if(auto json = json::parse(message)) {
                schedule(server.callback(std::move(*json), server.writer).handle());
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
}

}  // namespace

Server::Server(Callback callback) : callback(std::move(callback)) {
    static uv_pipe_t in;
    static uv_pipe_t out;

    writer.handle = &out;

    uv_pipe_init(loop, &in, 0);
    uv_pipe_init(loop, &out, 0);

    in.data = this;
    out.data = this;

    int r = uv_read_start((uv_stream_t*)&in, onAlloc, onRead);
}

Server::Server(Callback callback, const char* ip, unsigned int port) :
    callback(std::move(callback)) {
    static uv_tcp_t server;
    static uv_tcp_t client;

    writer.handle = &client;

    uv_tcp_init(loop, &server);
    uv_tcp_init(loop, &client);

    server.data = this;
    client.data = this;

    struct sockaddr_in addr;
    uv_ip4_addr(ip, port, &addr);

    uv_tcp_bind(&server, (const struct sockaddr*)&addr, 0);
    int r = uv_listen((uv_stream_t*)&server, 128, [](uv_stream_t* server, int status) {
        if(status < 0) {
            llvm::errs() << "New connection error\n";
            return;
        }

        if(uv_accept(server, (uv_stream_t*)&client) == 0) {
            llvm::errs() << "Client connected.\n";
            uv_read_start((uv_stream_t*)&client, onAlloc, onRead);
        } else {
            uv_close((uv_handle_t*)&client, NULL);
        }
    });
}

void Writer::write(json::Value id, json::Value result) {
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

    int r = uv_write(req, static_cast<uv_stream_t*>(handle), bufs, 2, on_write);

    if(r) {
        llvm::errs() << "Write error: " << uv_strerror(r) << "\n";
    }
}

}  // namespace clice::async
