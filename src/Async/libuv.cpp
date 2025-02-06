#include "Async/libuv.h"

namespace clice::async {

struct uv_error_category : public std::error_category {
    const char* name() const noexcept override {
        return "libuv";
    }

    std::string message(int code) const override {
#define UV_ERROR_HANDLE(name, message)                                                             \
    case UV_##name: return message;

        switch(code) {
            UV_ERRNO_MAP(UV_ERROR_HANDLE)
            default: return "unknown error";
        }

#undef UV_ERROR_HANDLE
    }
};

const std::error_category& category() {
    static uv_error_category instance;
    return instance;
}

}  // namespace clice::async
