#pragma once

#include "TExpr.h"
#include "LocationChain.h"
#include "Support/JSON.h"
#include "Support/Format.h"
#include "Support/Compare.h"
#include "Support/FileSystem.h"
#include "Support/FixedString.h"
#include "llvm/ADT/FunctionExtras.h"

namespace clice::testing {

class Runner {
public:
    static Runner& instance();

    using Suite = void (*)();
    using Test = llvm::unique_function<void()>;

    void add_suite(std::string_view name, Suite suite);

    void on_test(std::string_view name, Test test, bool skipped);

    void on_skip(std::string_view name);

    /// Current test is failed, continue to execute the next test in the suite.
    void fail(std::string expression, std::source_location location);

    /// Current test is fatal error, exit.
    void fatal(std::string expression, std::source_location location);

    /// Run all test suites.
    int run_tests();

private:
    Runner() = default;
    Runner(const Runner&) = delete;
    Runner(Runner&&) = delete;

private:
    bool failed = false;
    bool skipped = false;

    /// Whether all tests in this test suite are skipped.
    bool all_skipped = true;

    std::string curr_suite_name;
    std::uint32_t curr_tests_count = 0;
    std::uint32_t total_tests_count = 0;
    std::uint32_t total_suites_count = 0;
    std::chrono::milliseconds curr_test_duration;
    std::chrono::milliseconds totol_test_duration;
    std::unordered_map<std::string_view, std::vector<Suite>> suites;
};

template <fixed_string suite_name>
struct suite {
    template <typename Suite>
    suite(Suite suite) {
        static_assert(std::convertible_to<Suite, Runner::Suite>, "Suite must be stateless!");
        Runner::instance().add_suite(suite_name, suite);
    }
};

template <typename TExpr>
void expect(const TExpr& expr, std::source_location location = std::source_location::current()) {
    if constexpr(is_expr_v<TExpr>) {
        auto result = expr();
        if(!static_cast<bool>(result)) {
            /// TODO: use pretty print, if the expression is too long.
            Runner::instance().fail(std::format("{}", expr), location);
        }
    } else {
        if(!static_cast<bool>(expr)) {
            Runner::instance().fail("false", location);
        }
    }
}

struct test {
    test(std::string_view name) : name(name) {}

    template <typename Test>
    void operator= (Test&& test) {
        Runner::instance().on_test(name, std::forward<Test>(test), skipped);
    }

    bool skipped = false;
    std::string name;
};

struct skip_t {
    test&& operator/ (test&& test) {
        test.skipped = true;
        return std::move(test);
    }
};

inline skip_t skip;

struct that_t {
    template <typename TExpr>
    constexpr decltype(auto) operator% (const TExpr& expr) const {
        return expr;
    }
};

inline that_t that;

}  // namespace clice::testing
