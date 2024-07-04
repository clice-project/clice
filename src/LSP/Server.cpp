#include <simdjson.h>
#include <LSP/Server.h>

namespace clice {

Server server;

int Server::initialize() {
    loop = uv_default_loop();

    // initialize pipe and bind to stdin
    uv_pipe_init(loop, &stdin_pipe, 0);
    uv_pipe_open(&stdin_pipe, 0);

    auto alloc_buffer = [](uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
        buf->base = (char*)std::malloc(suggested_size);
        buf->len = suggested_size;
    };

    auto on_read = [](uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
        if(nread > 0) {
            server.handle_message(std::string_view(buf->base, nread));
        } else if(nread < 0) {
            // TODO: error handling
        }
        std::free(buf->base);
    };

    uv_read_start((uv_stream_t*)&stdin_pipe, alloc_buffer, on_read);

    // start the event loop
    return uv_run(loop, UV_RUN_DEFAULT);
}

int Server::exit() {
    // close the pipe
    // uv_close((uv_handle_t*)&stdin_pipe, NULL);

    // stop the event loop
    uv_stop(loop);
    return 0;
}

void Server::handle_message(std::string_view message) {
    const char* json = R"({
        "name": "John",
        "age": 30,
        "city": "New York"
    })";

    simdjson::dom::parser parser;

}  // namespace clice

}  // namespace clice
