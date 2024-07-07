#include <LSP/Server.h>
#include <LSP/Transport.h>
#include <Support/Logger.h>

namespace {

/// a helper class used to read and write messages for LSP
class Buffer {
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

}  // namespace

namespace clice {

void Pipe::alloc_buffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
    buf->base = (char*)std::malloc(suggested_size);
    buf->len = suggested_size;
}

void Pipe::on_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
    static Buffer buffer = {};
    if(nread > 0) {
        /// NOTICE: on_read function is always running in the main thread
        /// so there is no need to worry about thread safety
        buffer.write(std::string_view(buf->base, nread));

        /// read the message from the buffer
        if(std::string_view message = buffer.read(); !message.empty()) {
            /// if the message is not empty, handle it
            logger::info("read message: {}", message);
            server.handle_message(message);
            buffer.clear();
        }
    } else if(nread == UV_EOF) {
        uv_read_stop(stream);
    } else if(nread < 0) {
        logger::error("error reading from stream: {}", uv_strerror(nread));
        uv_read_stop(stream);
    }

    /// free the buffer
    free(buf->base);
}

void Pipe::send(std::string_view message) {
    std::string header = "Content-Length: " + std::to_string(message.size()) + "\r\n\r\n";
    header += message;
    uv_buf_t buf = uv_buf_init(header.data(), header.size());

    uv_write_t* req = (uv_write_t*)std::malloc(sizeof(uv_write_t));
    uv_write(req, (uv_stream_t*)&stdout_pipe, &buf, 1, [](uv_write_t* req, int status) {
        if(status < 0) {
            logger::error("error writing to stream: {}", uv_strerror(status));
        }
        std::free(req);
    });
}

}  // namespace clice
