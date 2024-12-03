#pragma once

#include "Struct.h"

namespace clice::support {

constexpr auto equal(const auto& lhs, const auto& rhs) {
    if constexpr(requires { lhs == rhs; }) {
        return (lhs == rhs);
    } else {
        return support::foreach(lhs, rhs, [](const auto& lhs, const auto& rhs) {
            return support::equal(lhs, rhs);
        });
    }
}

/// Compare lhs and rhs according to the dictionary order of their members.
constexpr auto less(const auto& lhs, const auto& rhs) {
    if constexpr(requires { lhs < rhs; }) {
        return (lhs < rhs);
    } else {
        bool result = false;
        support::foreach(lhs, rhs, [&result](const auto& lhs, const auto& rhs) {
            /// if lhs less than rhs, abort the iteration and return true.
            if(support::less(lhs, rhs)) {
                result = true;
                return false;
            }

            /// if lhs equal to rhs, continue to next member.
            if(support::equal(lhs, rhs)) {
                return true;
            }

            /// if lhs greater than rhs, abort the iteration and return false.
            return false;
        });
        return result;
    }
}

}  // namespace clice::support
