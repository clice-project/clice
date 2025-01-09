#include <deque>

#include "Server/Async2.h"

namespace clice::async2 {

namespace {

/// The default event loop.
uv_loop_t* loop = uv_default_loop();

/// The task queue waiting for resuming.
std::deque<std::coroutine_handle<>> tasks;

}  // namespace

/// This function is called by the event loop to resume the tasks.
static void event_loop(uv_idle_t* handle) {
    if(tasks.empty()) {
        return;
    }

    auto task = tasks.front();
    tasks.pop_front();
    task.resume();

    if(tasks.empty()) {
        uv_stop(loop);
    }
}

void schedule(std::coroutine_handle<> core) {
    assert(core && !core.done() && "schedule: invalid coroutine handle");
    tasks.emplace_back(core);
}

void run() {
    uv_idle_t idle;
    uv_idle_init(loop, &idle);
    uv_idle_start(&idle, event_loop);

    uv_run(loop, UV_RUN_DEFAULT);
}

}  // namespace clice::async2
