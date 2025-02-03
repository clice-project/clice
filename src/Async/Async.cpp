#include <deque>

#include "Async/Async.h"
#include "Support/Logger.h"

namespace clice::async {

/// The default event loop.
uv_loop_t* loop = uv_default_loop();

namespace {

/// The task queue waiting for resuming.
std::deque<std::coroutine_handle<>> tasks;

net::Callback callback = {};

uv_stream_t* writer = {};

/// Whether the server is listening.
bool listened = false;

}  // namespace

void schedule(std::coroutine_handle<> core) {
    uv_async_t* async = new uv_async_t;
    async->data = core.address();
    uv_async_init(loop, async, [](uv_async_t* handle) {
        auto core = std::coroutine_handle<>::from_address(handle->data);
        core.resume();
        uv_close((uv_handle_t*)handle, [](uv_handle_t* handle) { delete (uv_async_t*)handle; });
    });
    uv_async_send(async);
}

void run() {
#ifdef _WIN32
    _putenv_s("UV_THREADPOOL_SIZE", "20");
#else
    setenv("UV_THREADPOOL_SIZE", "20", 1);
#endif
    uv_run(loop, UV_RUN_DEFAULT);
}

}  // namespace clice::async
