#pragma once

#include "gtest/gtest.h"
#include "Basic/Location.h"
#include "llvm/ADT/StringMap.h"
#include "Support/Support.h"

namespace clice::testing {

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
        std::string expect;
        if constexpr(requires { sizeof(json::Serde<LHS>::serialize); }) {
            llvm::raw_string_ostream(expect) << json::serialize(lhs);
        } else {
            expect = "cannot dump value";
        }

        std::string actual;
        if constexpr(requires { sizeof(json::Serde<RHS>::serialize); }) {
            llvm::raw_string_ostream(actual) << json::serialize(rhs);
        } else {
            actual = "cannot dump value";
        }

        EXPECT_FAILURE(std::format("expect: {}, actual: {}\n", expect, actual), current);
    }
}

template <typename LHS, typename RHS>
inline void EXPECT_NE(const LHS& lhs,
                      const RHS& rhs,
                      std::source_location current = std::source_location::current()) {
    if(refl::equal(lhs, rhs)) {
        std::string expect;
        if constexpr(requires { sizeof(json::Serde<LHS>); }) {
            llvm::raw_string_ostream(expect) << json::serialize(lhs);
        } else {
            expect = "cannot dump value";
        }

        std::string actual;
        if constexpr(requires { sizeof(json::Serde<RHS>); }) {
            llvm::raw_string_ostream(actual) << json::serialize(rhs);
        } else {
            actual = "cannot dump value";
        }

        EXPECT_FAILURE(std::format("expect: {}, actual: {}\n", expect, actual), current);
    }
}

class Annotation {
public:
    Annotation(llvm::StringRef source) : m_source() {
        m_source.reserve(source.size());

        uint32_t line = 0;
        uint32_t column = 0;

        for(uint32_t i = 0; i < source.size();) {
            auto c = source[i];

            if(c == '@') {
                i += 1;
                auto key = source.substr(i).take_until([](char c) { return c == ' '; });
                assert(!locations.contains(key) && "duplicate key");
                locations.try_emplace(key, line, column);
                continue;
            }

            if(c == '$') {
                assert(i + 1 < source.size() && source[i + 1] == '(' && "expect $(name)");
                i += 2;
                auto key = source.substr(i).take_until([](char c) { return c == ')'; });
                i += key.size() + 1;
                assert(!locations.contains(key) && "duplicate key");
                locations.try_emplace(key, line, column);
                continue;
            }

            if(c == '\n') {
                line += 1;
                column = 0;
            } else {
                column += 1;
            }

            i += 1;
            m_source.push_back(c);
        }
    }

    Annotation(const Annotation&) = delete;

    Annotation(Annotation&& other) noexcept :
        m_source(std::move(other.m_source)), locations(std::move(other.locations)) {}

    Annotation& operator= (const Annotation&) = delete;

    Annotation& operator= (Annotation&& other) noexcept {
        m_source = std::move(other.m_source);
        locations = std::move(other.locations);
        return *this;
    }

    llvm::StringRef source() const {
        return m_source;
    }

    proto::Position pos(llvm::StringRef key) const {
        return locations.lookup(key);
    }

private:
    std::string m_source;
    llvm::StringMap<proto::Position> locations;
};

}  // namespace clice::testing
