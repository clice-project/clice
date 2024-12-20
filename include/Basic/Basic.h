#pragma once

#include <vector>

#include "llvm/ADT/StringRef.h"

namespace clice::proto {

/// range in [-2^31, 2^31- 1]
using integer = std::int32_t;

/// range in [0, 2^31- 1]
using uinteger = std::uint32_t;

using string = std::string;

using string_literal = llvm::StringLiteral;

template <typename T>
using array = std::vector<T>;

using DocumentUri = std::string;

// TODO: figure out URI.
using URI = std::string;

}  // namespace clice::proto
