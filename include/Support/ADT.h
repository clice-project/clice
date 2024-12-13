#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <compare>
#include <ranges>
#include <expected>

#include <llvm/ADT/ArrayRef.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/ADT/StringMap.h>
#include <llvm/ADT/StringExtras.h>

namespace clice {

template <typename T>
using expected = std::expected<T, std::string>;

namespace ranges = std::ranges;
namespace views = std::ranges::views;

}  // namespace clice
