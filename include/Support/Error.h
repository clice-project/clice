#pragma once

#include <llvm/Support/Error.h>
#include <llvm/Support/FormatVariadic.h>

namespace clice {

template <typename... Args>
llvm::Error error(const char* fmt, Args&&... args) {
    return llvm::make_error<llvm::StringError>(llvm::formatv(fmt, std::forward<Args>(args)...).str(),
                                               llvm::inconvertibleErrorCode());
}

}  // namespace clice
