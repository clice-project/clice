#pragma once

#include "gtest/gtest.h"
#include "Support/JSON.h"
#include "Support/Format.h"
#include "Support/Compare.h"
#include "Support/FileSystem.h"
#include "Test/LocationChain.h"

namespace clice::testing {

#undef EXPECT_TRUE
#undef EXPECT_FALSE
#undef ASSERT_TRUE
#undef ASSERT_FALSE
#undef EXPECT_EQ
#undef EXPECT_NE
#undef ASSERT_EQ
#undef ASSERT_NE

llvm::StringRef test_dir();

template <typename LHS, typename RHS>
inline std::string diff(const LHS& lhs, const RHS& rhs) {
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

    return std::format("left : {}\nright: {}\n", left, right);
}

inline void EXPECT_FAILURE(std::string message, LocationChain chain = LocationChain()) {
    chain.backtrace();
    GTEST_MESSAGE_AT_("", 0, message.c_str(), ::testing::TestPartResult::kNonFatalFailure);
}

inline void ASSERT_FAILURE(std::string message, LocationChain chain = LocationChain()) {
    chain.backtrace();
    GTEST_MESSAGE_AT_("", 0, message.c_str(), ::testing::TestPartResult::kFatalFailure);
}

inline void EXPECT_TRUE(auto&& value, LocationChain chain = LocationChain()) {
    if(!static_cast<bool>(value)) {
        EXPECT_FAILURE("EXPECT true!", chain);
    }
}

inline void EXPECT_FALSE(auto&& value, LocationChain chain = LocationChain()) {
    if(static_cast<bool>(value)) {
        EXPECT_FAILURE("EXPECT false!", chain);
    }
}

inline void ASSERT_TRUE(auto&& value, LocationChain chain = LocationChain()) {
    if(!static_cast<bool>(value)) {
        ASSERT_FAILURE("ASSERT true!", chain);
        if constexpr(requires { value.error(); }) {
            clice::println("{}", value.error());
        }
    }
}

inline void ASSERT_FALSE(auto&& value, LocationChain chain = LocationChain()) {
    if(static_cast<bool>(value)) {
        ASSERT_FAILURE("ASSERT false!", chain);
    }
}

template <typename LHS, typename RHS>
inline void EXPECT_EQ(const LHS& lhs, const RHS& rhs, LocationChain chain = LocationChain()) {
    if(!refl::equal(lhs, rhs)) {
        EXPECT_FAILURE(diff(lhs, rhs), chain);
    }
}

template <typename LHS, typename RHS>
inline void EXPECT_NE(const LHS& lhs, const RHS& rhs, LocationChain chain = LocationChain()) {
    if(refl::equal(lhs, rhs)) {
        EXPECT_FAILURE(diff(lhs, rhs), chain);
    }
}

template <typename LHS, typename RHS>
inline void ASSERT_EQ(const LHS& lhs, const RHS& rhs, LocationChain chain = LocationChain()) {
    if(!refl::equal(lhs, rhs)) {
        ASSERT_FAILURE(diff(lhs, rhs), chain);
    }
}

template <typename LHS, typename RHS>
inline void ASSERT_NE(const LHS& lhs, const RHS& rhs, LocationChain chain = LocationChain()) {
    if(refl::equal(lhs, rhs)) {
        ASSERT_FAILURE(diff(lhs, rhs), chain);
    }
}

}  // namespace clice::testing
