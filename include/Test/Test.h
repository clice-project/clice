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

    void run_test(std::string_view name, Test test);

    /// Current test is failed, continue to execute the next test in the suite.
    void fail(std::string expression, std::source_location location);

    /// Current test is fatal error, exit.
    void fatal(std::string expression, std::source_location location);

    int run_tests();

private:
    Runner() = default;
    Runner(const Runner&) = delete;
    Runner(Runner&&) = delete;

private:
    bool failed = false;
    std::string curr_suite_name;
    std::uint32_t curr_tests_count = 0;
    std::uint32_t total_tests_count = 0;
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
    if(!bool(expr)) {
        std::abort();
    }
}

struct test {
    test(std::string_view name) : name(name) {}

    template <typename Test>
    void operator= (Test&& test) {
        Runner::instance().run_test(name, std::forward<Test>(test));
    }

    std::string name;
};

struct that_t {
    template <typename TExpr>
    constexpr decltype(auto) operator% (const TExpr& expr) const {
        return expr;
    }
};

constexpr inline that_t that;

}  // namespace clice::testing
