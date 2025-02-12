#pragma once

#include "Task.h"
#include "Event.h"
#include "Gather.h"

namespace clice::async {

void run();

namespace awaiter {

template <typename Callback>
struct suspend {
    Callback callback;

    bool await_ready() noexcept {
        return false;
    }

    template <typename Promise>
    void await_suspend(std::coroutine_handle<Promise> handle) noexcept {
        callback(&handle.promise());
    }

    void await_resume() noexcept {}
};

}  // namespace awaiter

template <typename Callback>
auto suspend(Callback&& callback) {
    return awaiter::suspend<std::remove_cvref_t<Callback>>{std::forward<Callback>(callback)};
}

/// Run the tasks in parallel and return the results.
template <typename... Tasks>
auto run(Tasks&&... tasks) {
    auto core = gather(std::forward<Tasks>(tasks)...);
    core.schedule();
    async::run();
    assert(core.done() && "run: not done");
    return core.result();
}

};  // namespace clice::async

