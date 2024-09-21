#pragma once

#include <string>
#include <string_view>
#include <vector>

#include <llvm/ADT/ArrayRef.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/SmallString.h>

namespace clice {
using llvm::ArrayRef;
using llvm::StringRef;
using llvm::SmallVector;
using llvm::SmallString;
}  // namespace clice
