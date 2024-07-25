#pragma once

#include <uv.h>

namespace clice {

class Transport {
public:
    virtual void send(std::string_view) = 0;
    virtual ~Transport() = default;
};

class Pipe : public Transport {
private:
    uv_pipe_t stdin_pipe;
    uv_pipe_t stdout_pipe;

private:
    static void alloc_buffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf);
    static void on_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf);

public:
    Pipe() {
        // initialize the pipe
        uv_loop_t* loop = uv_default_loop();
        uv_pipe_init(loop, &stdin_pipe, 0);
        uv_pipe_init(loop, &stdout_pipe, 0);

        // bind to stdin and stdout
        uv_pipe_open(&stdin_pipe, 0);
        uv_pipe_open(&stdout_pipe, 1);

        // set callback for reading from stdin
        uv_read_start((uv_stream_t*)&stdin_pipe, alloc_buffer, on_read);
    }

    void send(std::string_view message) override;

    ~Pipe() override {
        uv_close((uv_handle_t*)&stdin_pipe, nullptr);
        uv_close((uv_handle_t*)&stdout_pipe, nullptr);
    }
};

class Socket : public Transport {
private:
    uv_tcp_t socket;

public:
    Socket(std::string_view address, int port);

    void send(std::string_view message) override;

    ~Socket() override { uv_close((uv_handle_t*)&socket, nullptr); }
};

}  // namespace clice
