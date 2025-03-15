#pragma once

#include "gtest/gtest.h"
#include "Basic/Location.h"
#include "Support/JSON.h"
#include "Support/Format.h"
#include "Support/Compare.h"
#include "Support/FileSystem.h"
#include "Annotation.h"

namespace clice::testing {

llvm::StringRef test_dir();

#undef EXPECT_EQ
#undef EXPECT_NE

inline void EXPECT_FAILURE(std::string msg,
                           std::source_location current = std::source_location::current()) {
    ::testing::internal::AssertHelper(::testing ::TestPartResult ::kNonFatalFailure,
                                      current.file_name(),
                                      current.line(),
                                      msg.c_str()) = ::testing ::Message();
}

template <typename LHS, typename RHS>
inline void EXPECT_EQ(const LHS& lhs,
                      const RHS& rhs,
                      std::source_location current = std::source_location::current()) {
    if(!refl::equal(lhs, rhs)) {
        std::string left;
        if constexpr(json::serializable<LHS>) {
            llvm::raw_string_ostream(left) << json::serialize(lhs);
        } else {
            left = "cannot dump value";
        }

        std::string right;
        if constexpr(json::serializable<RHS>) {
            llvm::raw_string_ostream(right) << json::serialize(rhs);
        } else {
            right = "cannot dump value";
        }

        EXPECT_FAILURE(std::format("left : {}\nright: {}\n", left, right), current);
    }
}

template <typename LHS, typename RHS>
inline void EXPECT_NE(const LHS& lhs,
                      const RHS& rhs,
                      std::source_location current = std::source_location::current()) {
    if(refl::equal(lhs, rhs)) {
        std::string expect;
        if constexpr(requires { json::Serde<LHS>::serialize; }) {
            llvm::raw_string_ostream(expect) << json::serialize(lhs);
        } else {
            expect = "cannot dump value";
        }

        std::string actual;
        if constexpr(requires { json::Serde<LHS>::serialize; }) {
            llvm::raw_string_ostream(actual) << json::serialize(rhs);
        } else {
            actual = "cannot dump value";
        }

        EXPECT_FAILURE(std::format("expect: {}, actual: {}\n", expect, actual), current);
    }
}

}  // namespace clice::testing
