#pragma once

#include <llvm/ADT/StringRef.h>
#include <llvm/ADT/StringExtras.h>

namespace clice {

class Compiler;

}

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

/// Beacuse C++ does support string enum, so define `enum_type` for
/// tag when serialize/deserialize.
template <typename T>
struct enum_type {
    T value;

    using underlying_type = T;

    constexpr enum_type(T value) : value(value) {}

    friend bool operator== (const enum_type& lhs, const enum_type& rhs) = default;
};

}  // namespace clice::proto
