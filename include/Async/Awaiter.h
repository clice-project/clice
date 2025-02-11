#pragma once

#include <expected>

#include "Task.h"
#include "libuv.h"

namespace clice::async::awaiter {

template <typename Request>
struct uv_base;

template <typename Request>
    requires (is_uv_handle_v<Request>)
struct uv_base<Request> {
    Request& request;

    uv_base() : request(*static_cast<Request*>(std::malloc(sizeof(Request)))) {}

    ~uv_base() {
        /// For libuv handles, we must call uv_close to release resources.
        /// However, uv_close is a async operation, if we store the handle in the promise object,
        /// it may be destroyed before the uv_close operation is finished.
        /// We decide to alloc for it separately and free it in the callback function.
        uv_close(reinterpret_cast<uv_handle_t*>(&request),
                 [](uv_handle_t* handle) { std::free(handle); });
    }
};

template <typename Request>
    requires (is_uv_req_v<Request>)
struct uv_base<Request> {
    /// For libuv requests, we don't need to release resources, so we can store it in the promise
    /// object.
    Request request;
};

/// The CRTP base class for the awaiter of libuv async operations. The Derived should
/// implement the `start` and `cleanup` functions.
template <typename Derived, typename Request, typename Ret, typename... Extras>
struct uv : uv_base<Request> {
    int error = 0;
    promise_base* continuation;

    bool await_ready() const noexcept {
        return false;
    }

    /// The callback function to handle the async operation. This should always called
    /// in the main thread.
    static void callback(Request* request, Extras... extras) {
        auto& self = *static_cast<Derived*>(request->data);

        /// The derived should implement the cleanup function to release resources or set
        /// the error code if the async operation fails.
        self.cleanup(extras...);

        /// Then we resume the coroutine. It may destroy the current task,
        /// If the task is cancelled and disposable.
        self.continuation->resume();
    }

    template <typename Promise>
    std::coroutine_handle<> await_suspend(std::coroutine_handle<Promise> waiting) noexcept {
        continuation = &waiting.promise();
        this->request.data = static_cast<Derived*>(this);

        auto& self = *static_cast<Derived*>(this);

        /// Start the async operation.
        error = self.start(callback);

        /// If the async operation fails, resume the coroutine immediately.
        if(error < 0) {
            return continuation->resume_handle();
        }

        /// Otherwise, return the coroutine handle to resume later.
        return std::noop_coroutine();
    }

    std::expected<Ret, std::error_code> await_resume() noexcept {
        if(error < 0) {
            return std::unexpected(std::error_code(error, category()));
        }

        if constexpr(!std::is_void_v<Ret>) {
            return static_cast<Derived*>(this)->result();
        } else {
            return std::expected<void, std::error_code>();
        }
    }
};

}  // namespace clice::async::awaiter
