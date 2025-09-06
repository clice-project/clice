#pragma once

#include "llvm/ADT/StringRef.h"

namespace clice::tidy {

bool is_registered_tidy_check(llvm::StringRef check);
std::optional<bool> is_fast_tidy_check(llvm::StringRef check);

}  // namespace clice::tidy
