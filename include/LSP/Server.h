#include <uv.h>
#include <string_view>

namespace clice {

// global server instance
extern class Server server;

/// core class responsible for starting the server
class Server {
    uv_loop_t* loop;
    uv_pipe_t stdin_pipe;

public:
    int start();
    int exit();
    void handle_message(std::string_view message);

    // LSP methods
    void initialize();
};

}  // namespace clice
