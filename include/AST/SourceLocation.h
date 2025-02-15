#pragma once

#include "clang/Basic/SourceLocation.h"

namespace std {

template <>
struct tuple_size<clang::SourceRange> : std::integral_constant<std::size_t, 2> {};

template <>
struct tuple_element<0, clang::SourceRange> {
    using type = clang::SourceLocation;
};

template <>
struct tuple_element<1, clang::SourceRange> {
    using type = clang::SourceLocation;
};

}  // namespace std

namespace clang {

/// Through ADL, make `clang::SourceRange` could be destructured.
template <std::size_t I>
clang::SourceLocation get(clang::SourceRange range) {
    if constexpr(I == 0) {
        return range.getBegin();
    } else {
        return range.getEnd();
    }
}

}  // namespace clang

