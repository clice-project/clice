#include <deque>

#include "Async/Async.h"
#include "Support/Logger.h"

namespace clice::async {

/// Check the result of a libuv function call and log an error if it failed.
/// Use source_location to log the file, line, and function name where the error occurred.
void uv_check_result(const int result, const std::source_location location = std::source_location::current()) {
    if(result < 0) {
        log::warn("libuv error: {}", uv_strerror(result));
        log::warn("At {}:{}:{}", 
                  location.file_name(), 
                  location.line(),
                  location.function_name());
    }
}

/// The default event loop.
uv_loop_t* loop = nullptr;

namespace {

uv_loop_t instance;
uv_idle_t idle;
bool idle_running = false;
std::deque<promise_base*> tasks;

void each(uv_idle_t* idle) {
    if(idle_running && tasks.empty()) {
        idle_running = false;
        uv_check_result(uv_idle_stop(idle));
    }

    /// Resume may create new tasks, we want to run them in the next iteration.
    auto all = std::move(tasks);
    for(auto& task: all) {
        task->resume();
    }
}

}  // namespace

void promise_base::schedule() {
    if(loop && !idle_running && tasks.empty()) {
        idle_running = true;
        uv_check_result(uv_idle_start(&idle, each));
    }

    tasks.push_back(this);
}

void init() {
    loop = &instance;

    uv_check_result(uv_loop_init(loop));

    idle_running = true;
    uv_check_result(uv_idle_init(loop, &idle));
    uv_check_result(uv_idle_start(&idle, each));
}

void run() {
    if(!loop) {
        init();
    }

    uv_check_result(uv_os_setenv("UV_THREADPOOL_SIZE", "20"));

    uv_check_result(uv_run(loop, UV_RUN_DEFAULT));

    uv_close(reinterpret_cast<uv_handle_t*>(&idle), nullptr);

    /// Run agian to cleanup the loop.
    uv_check_result(uv_run(loop, UV_RUN_DEFAULT));
    uv_check_result(uv_loop_close(loop));

    loop = nullptr;
}

}  // namespace clice::async
