#pragma once

#include "Format.h"

namespace clice {

#ifndef NDEBUG
#define ASSERT(expr, message, ...)                                                                 \
    if(!(expr)) {                                                                                  \
        llvm::errs() << "ASSERT FAIL: " << std::format(message, ##__VA_ARGS__);                    \
        std::abort();                                                                              \
    }
#else
#define ASSERT(expr, message)
#endif

}  // namespace clice
