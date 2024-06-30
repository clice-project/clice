#include <uv.h>
#include <LSP/Server.h>

namespace clice {

int Server::run() { 
    uv_loop_t* loop = uv_default_loop();
    return 0; 
}

} // namespace clice