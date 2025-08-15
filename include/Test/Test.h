#pragma once

#include "TExpr.h"
#include "LocationChain.h"
#include "Support/JSON.h"
#include "Support/Format.h"
#include "Support/Compare.h"
#include "Support/FileSystem.h"
#include "Support/FixedString.h"

namespace clice::testing {

using TSuite = void (*)();

void add_suite(std::string_view name, TSuite suite);

template <fixed_string suite_name>
struct suite {
    template <typename Suite>
    suite(Suite suite) {
        static_assert(std::convertible_to<Suite, TSuite>, "Suite must be stateless!");
        add_suite(suite_name, suite);
    }
};

template <typename TExpr>
void expect(const TExpr& expr, std::source_location location = std::source_location::current()) {
    if(!bool(expr)) {
        std::abort();
    }
}

struct test {
    test(std::string_view name) {}

    template <typename Test>
    void operator= (Test&& test) {
        test();
    }
};

struct that_t {
    template <typename TExpr>
    constexpr decltype(auto) operator% (const TExpr& expr) const {
        return expr;
    }
};

constexpr inline that_t that;

}  // namespace clice::testing
