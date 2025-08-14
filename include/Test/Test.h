#pragma once

#include "boost/ut.hpp"
#include "Support/JSON.h"
#include "Support/Format.h"
#include "Support/Compare.h"
#include "Support/FileSystem.h"
#include "Test/LocationChain.h"

namespace std {

/// FIXME: Use a better way to print optional.
template <typename T>
ostream& operator<< (std::ostream& os, const optional<T>& t) {
    if(t) {
        os << *t;
    } else {
        os << "nullopt";
    }
    return os;
}

}  // namespace std

namespace clice::testing {

using namespace boost::ut;

}  // namespace clice::testing
