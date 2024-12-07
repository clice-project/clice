#pragma once

#include "Support/ADT.h"
#include "Struct.h"

namespace clice::support {

template <typename LHS, typename RHS = LHS>
struct Equal {
    static bool equal(const LHS& lhs, const RHS& rhs) {
        return lhs == rhs;
    }
};

template <typename LHS, typename RHS>
bool equal(const LHS& lhs, const RHS& rhs) {
    return Equal<LHS, RHS>::equal(lhs, rhs);
}

template <typename T>
struct Equal<std::vector<T>> {
    static bool equal(const std::vector<T>& lhs, const std::vector<T>& rhs) {
        if(lhs.size() != rhs.size()) {
            return false;
        }

        for(std::size_t i = 0; i < lhs.size(); ++i) {
            if(!support::equal(lhs[i], rhs[i])) {
                return false;
            }
        }

        return true;
    }
};

template <reflectable T>
    requires (!requires(T lhs, T rhs) {
        { lhs == rhs } -> std::convertible_to<bool>;
    })
struct Equal<T> {
    static bool equal(const T& lhs, const T& rhs) {
        return foreach(lhs, rhs, [](const auto& lhs, const auto& rhs) {
            return support::equal(lhs, rhs);
        });
    }
};

}  // namespace clice::support
