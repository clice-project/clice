#include <LSP/Server.h>

namespace clice {

uv_loop_t* Server::loop;
uv_pipe_t Server::stdin_pipe;

static void on_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
    if(nread > 0) {
        printf("Read: %s\n", buf->base);
    } else if(nread < 0) {
        if(nread != UV_EOF) {
            fprintf(stderr, "Read error: %s\n", uv_err_name(nread));
        }

        if(!uv_is_closing((uv_handle_t*)&pipe)) {
            uv_close((uv_handle_t*)&pipe, NULL);
        }
    }

    if(buf->base) {
        free(buf->base);
    }
}

static void alloc_buffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
    buf->base = (char*)malloc(suggested_size);
    buf->len = suggested_size;
}

int Server::Initialize() {
    loop = uv_default_loop();
    uv_pipe_init(loop, &stdin_pipe, 0);
    uv_pipe_open(&stdin_pipe, 0);
    uv_read_start((uv_stream_t*)&stdin_pipe, alloc_buffer, on_read);
    return uv_run(loop, UV_RUN_DEFAULT);
}

int Server::Exit() {
    uv_close((uv_handle_t*)&stdin_pipe, NULL);
    return uv_run(loop, UV_RUN_DEFAULT);
}

}  // namespace clice
