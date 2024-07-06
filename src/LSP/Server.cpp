#include <LSP/Server.h>
#include <LSP/Protocol.h>
#include <LSP/MessageBuffer.h>

#include <Support/Logger.h>
#include <Support/Serialize.h>

namespace clice {

Server server;
static MessageBuffer buffer;

void on_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
    if(nread > 0) {
        buffer.write(std::string_view(buf->base, nread));
        if(auto message = buffer.read(); !message.empty()) {
            server.handle_message(message);
            buffer.clear();
        }
    } else if(nread == UV_EOF) {
        uv_read_stop(stream);
    } else if(nread < 0) {
        uv_read_stop(stream);
    }
    free(buf->base);
}

int Server::start() {
    loop = uv_default_loop();

    // initialize pipe and bind to stdin
    uv_pipe_init(loop, &stdin_pipe, 0);
    uv_pipe_init(loop, &stdout_pipe, 0);
    uv_pipe_open(&stdin_pipe, 0);
    uv_pipe_open(&stdout_pipe, 1);

    auto alloc_buffer = [](uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
        buf->base = (char*)std::malloc(suggested_size);
        buf->len = suggested_size;
    };

    uv_read_start((uv_stream_t*)&stdin_pipe, alloc_buffer, on_read);

    // start the event loop
    return uv_run(loop, UV_RUN_DEFAULT);
}

int Server::exit() {
    // uv_pipe_end(&stdin_pipe);
    // uv_pipe_end(&stdout_pipe);

    // stop the event loop
    uv_stop(loop);
    return 0;
}

void Server::handle_message(std::string_view message) {
    try {
        auto input = json::parse(message);
        int id = input["id"].get<int>();
        std::string_view method = input["method"].get<std::string_view>();
        logger::info("method: {}", method);

        if(method == "initialize") {
            // initialize();
            json json = {
                {"id",     id                                  },
                {"result", clice::serialize(InitializeResult{})}
            };

            auto stream = json.dump();
            std::string header = "Content-Length: " + std::to_string(stream.size()) + "\r\n\r\n";
            header += stream;

            uv_buf_t buf = uv_buf_init(header.data(), header.size());
            uv_write_t* req = (uv_write_t*)malloc(sizeof(uv_write_t));
            uv_write(req, (uv_stream_t*)&stdout_pipe, &buf, 1, NULL);
        }
    } catch(std::exception& e) {
        logger::error("failed to parse JSON: {}", e.what());
        return;
    }
}

}  // namespace clice
