#pragma once

#include "llvm/ADT/StringRef.h"

namespace clice::tidy {

bool isRegisteredTidyCheck(llvm::StringRef Check);
std::optional<bool> isFastTidyCheck(llvm::StringRef Check);

}  // namespace clice::tidy
