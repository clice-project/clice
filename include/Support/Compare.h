#pragma once

#include "Support/ADT.h"

namespace clice::support {

template <typename T>
struct Equal {
    static bool equal(const T& lhs, const T& rhs) {
        return lhs == rhs;
    }
};

template <typename T>
bool equal(const T& lhs, const T& rhs) {
    return Equal<T>::equal(lhs, rhs);
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

}  // namespace clice::support
