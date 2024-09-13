#pragma once

#include <uv.h>
#include <string_view>

namespace clice {

class Transport {
public:
    virtual void write(std::string_view message) = 0;
};

class Pipe : public Transport {
public:
    Pipe(uv_loop_t* loop, void (*callback)(std::string_view));

    void write(std::string_view message) override;

private:
    uv_pipe_t stdin_pipe;
    uv_pipe_t stdout_pipe;
};

class Socket : public Transport {
public:
    Socket(uv_loop_t* loop, void (*callback)(std::string_view), const char* host, unsigned int port);

    void write(std::string_view message) override;

private:
    uv_tcp_t server;
};

}  // namespace clice
