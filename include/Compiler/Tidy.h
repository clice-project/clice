#pragma once

#include "llvm/ADT/StringRef.h"

namespace clice::tidy {

bool isRegisteredTidyCheck(llvm::StringRef check);
std::optional<bool> isFastTidyCheck(llvm::StringRef check);

}  // namespace clice::tidy
