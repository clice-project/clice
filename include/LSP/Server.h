#include <uv.h>

namespace clice {

/// core class responsible for starting the server
class Server {
    static uv_loop_t* loop;
    static uv_pipe_t stdin_pipe;

public:
    static int Initialize();
    static int Exit();
};

}  // namespace clice
