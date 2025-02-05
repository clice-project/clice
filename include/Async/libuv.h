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

const std::error_category& category();

}  // namespace clice::async
