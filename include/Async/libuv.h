#pragma once

#ifdef _WIN32
#define NOMINMAX
#endif

#include "uv.h"

#ifdef _WIN32
#undef THIS
#endif

#include <cassert>
#include <expected>
#include <type_traits>
#include <system_error>

namespace clice::async {

/// The default event loop.
extern uv_loop_t* loop;

template <typename T, typename U>
T& uv_cast(U* u) {
    assert(u && u->data && "uv_cast: invalid uv handle");
    return *static_cast<std::remove_cvref_t<T>*>(u->data);
}

#define UV_TYPE_ITER(_, name) || std::is_same_v<T, uv_##name##_t>

/// Check if the type `T` is a libuv handle.
template <typename T>
constexpr bool is_uv_handle_v = false UV_HANDLE_TYPE_MAP(UV_TYPE_ITER);

/// Check if the type `T` is a libuv request.
template <typename T>
constexpr bool is_uv_req_v = false UV_REQ_TYPE_MAP(UV_TYPE_ITER);

#undef UV_TYPE_ITER

template <typename T>
class Task;

template <typename T>
using Result = Task<std::expected<T, std::error_code>>;

const std::error_category& category();

void run();

}  // namespace clice::async
