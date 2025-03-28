#include <deque>

#include "Async/Async.h"
#include "Support/Logger.h"

namespace clice::async {

#define UV_CHECK_RESULT(expr)                                                  \
  do {                                                                         \
    int err = (expr);                                                          \
    if (err < 0) {                                                             \
      log::warn("lib uv error: {}", uv_strerror(err));                         \
      auto location = std::source_location::current();                         \
      log::warn("At {}:{}:{}", location.file_name(), location.line(),          \
                location.function_name());                                     \
    }                                                                          \
  } while (0)

/// The default event loop.
uv_loop_t *loop = nullptr;

namespace {

uv_loop_t instance;
uv_idle_t idle;
bool idle_running = false;
std::deque<promise_base *> tasks;

void each(uv_idle_t *idle) {
  if (idle_running && tasks.empty()) {
    idle_running = false;
    UV_CHECK_RESULT(uv_idle_stop(idle));
  }

  /// Resume may create new tasks, we want to run them in the next iteration.
  auto all = std::move(tasks);
  for (auto &task : all) {
    task->resume();
  }
}

} // namespace

void promise_base::schedule() {
  if (loop && !idle_running && tasks.empty()) {
    idle_running = true;
    UV_CHECK_RESULT(uv_idle_start(&idle, each));
  }

  tasks.push_back(this);
}

void init() {
  loop = &instance;

  UV_CHECK_RESULT(uv_loop_init(loop));

  idle_running = true;
  UV_CHECK_RESULT(uv_idle_init(loop, &idle));
  UV_CHECK_RESULT(uv_idle_start(&idle, each));
}

void run() {
  if (!loop) {
    init();
  }

  UV_CHECK_RESULT(uv_os_setenv("UV_THREADPOOL_SIZE", "20"));

  UV_CHECK_RESULT(uv_run(loop, UV_RUN_DEFAULT));

  uv_close(reinterpret_cast<uv_handle_t *>(&idle), nullptr);

  /// Run agian to cleanup the loop.
  UV_CHECK_RESULT(uv_run(loop, UV_RUN_DEFAULT));
  UV_CHECK_RESULT(uv_loop_close(loop));

  loop = nullptr;
}

} // namespace clice::async
