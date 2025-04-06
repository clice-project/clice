#pragma once

#include "Awaiter.h"

namespace clice::async {

namespace awaiter {

template <typename Ret>
struct value {
    std::optional<Ret> value;
};

template <>
struct value<void> {};

template <typename Work, typename Ret>
struct thread_pool : value<Ret>, uv<thread_pool<Work, Ret>, uv_work_t, Ret, int> {
    Work work;

    /// `uv_work_t` has two callback functions, `work_cb` is executed in the thread pool,
    /// and `after_work_cb` is executed in the main thread.
    static void work_cb(uv_work_t* work) {
        auto& awaiter = uv_cast<thread_pool>(work);
        if constexpr(!std::is_void_v<Ret>) {
            awaiter.value.emplace(awaiter.work());
        } else {
            awaiter.work();
        }
    }

    int start(auto callback) {
        return uv_queue_work(async::loop, &this->request, work_cb, callback);
    }

    void cleanup(int status) {
        this->error = status;
    }

    Ret result() {
        if constexpr(!std::is_void_v<Ret>) {
            return std::move(*this->value);
        }
    }
};

}  // namespace awaiter

template <typename Work, typename Ret = std::invoke_result_t<Work>>
async::Task<Ret> submit(Work&& work) {
    using W = std::remove_cvref_t<Work>;
    auto result = co_await awaiter::thread_pool<W, Ret>{{}, {}, std::forward<Work>(work)};
    if(!result) {
        /// Thread pool task should never fails.
        std::abort();
    }
    co_return std::move(*result);
}

}  // namespace clice::async
