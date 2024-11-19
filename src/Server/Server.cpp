#include "uv.h"
#include "Server/Config.h"
#include "Server/Server.h"
#include "Basic/Location.h"
#include "llvm/Support/raw_ostream.h"

namespace clice {

static uv_loop_t* loop;
static uv_idle_t idle;
static uv_stream_t* writer;

void schedule() {}

void on_alloc_buffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
    static llvm::SmallString<4096> buffer;
    buffer.resize_for_overwrite(suggested_size);
    buf->base = buffer.data();
    buf->len = suggested_size;
}

void on_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
    if(nread > 0) {
        llvm::outs() << "Received from stdin: " << llvm::StringRef(buf->base, nread);
    } else if(nread < 0) {
        if(nread != UV_EOF) {
            fprintf(stderr, "Error reading from stdin: %s\n", uv_strerror(nread));
        }
        uv_close((uv_handle_t*)stream, NULL);
    }
}

void on_write(uv_write_t* req, int status) {
    if(status < 0) {
        fprintf(stderr, "Write error: %s\n", uv_strerror(status));
    } else {
        printf("Write completed successfully.\n");
    }
    free(req);
}

void write(llvm::StringRef message) {
    /// FIXME:
    static uv_write_t write_req;
    uv_buf_t buf = uv_buf_init((char*)message.data(), message.size());
    uv_write(&write_req, writer, &buf, 1, on_write);
}

uv_stream_t* init_socket(const char* address, unsigned int port) {
    static uv_tcp_t server;
    static uv_tcp_t client;

    uv_tcp_init(loop, &server);
    uv_tcp_init(loop, &client);

    struct sockaddr_in addr;
    uv_ip4_addr(address, port, &addr);

    uv_tcp_bind(&server, (const struct sockaddr*)&addr, 0);

    int r = uv_listen((uv_stream_t*)&server, 128, [](uv_stream_t* server, int status) {
        if(status < 0) {
            fprintf(stderr, "New connection error\n");
            return;
        }

        if(uv_accept(server, (uv_stream_t*)&client) == 0) {
            printf("Client connected.\n");
            uv_read_start((uv_stream_t*)&client, on_alloc_buffer, on_read);
        } else {
            uv_close((uv_handle_t*)&client, NULL);
        }
    });

    if(r) {
        fprintf(stderr, "Listen error: %s\n", uv_strerror(r));
    }

    return (uv_stream_t*)&client;
}

uv_stream_t* init_pipe() {
    static uv_pipe_t stdin_pipe;
    static uv_pipe_t stdout_pipe;

    uv_pipe_init(loop, &stdin_pipe, 0);
    uv_pipe_init(loop, &stdout_pipe, 0);

    uv_pipe_open(&stdin_pipe, 0);
    uv_pipe_open(&stdout_pipe, 1);

    uv_read_start((uv_stream_t*)&stdin_pipe, on_alloc_buffer, on_read);

    return (uv_stream_t*)&stdout_pipe;
}

int run(int argc, const char** argv) {
    loop = uv_default_loop();

    uv_idle_init(loop, &idle);
    uv_idle_start(&idle, [](uv_idle_t* handle) { schedule(); });

    /// read config file.
    config::parse(argc, argv);

    /// init writer.
    const auto& option = config::server();
    if(option.mode == "socket") {
        writer = init_socket(option.address.c_str(), option.port);
    } else if(option.mode == "pipe") {
        writer = init_pipe();
    } else {
        llvm::errs() << "Unknown mode: " << option.mode << "\n";
        return 1;
    }

    return uv_run(loop, UV_RUN_DEFAULT);
}

}  // namespace clice
