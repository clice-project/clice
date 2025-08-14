#include "Test/Test.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Signals.h"

#include "boost/ut.hpp"
#include "Support/GlobPattern.h"

namespace clice {

namespace cl {

llvm::cl::opt<std::string> test_dir("test-dir",
                                    llvm::cl::desc("specify the test source directory path"),
                                    llvm::cl::value_desc("path"),
                                    llvm::cl::Required);

llvm::cl::opt<std::string> resource_dir("resource-dir", llvm::cl::desc("Resource dir path"));

llvm::cl::opt<std::string> test_filter("test_filter");

clice::GlobPattern filter_pattern;

}  // namespace cl

namespace testing {

llvm::StringRef test_dir() {
    return cl::test_dir;
}

}  // namespace testing

}  // namespace clice

namespace ut = boost::ut;
using namespace ut;
using namespace ut::literals;

namespace cfg {

class runner {

public:
    constexpr runner() = default;

    ~runner() {
        const auto should_run = not run_;

        if(should_run) {
            static_cast<void>(run());
        }

        report_summary();

        if(should_run && fails) {
            std::exit(-1);
        }
    }

    using TSuite = events::suite<void (*)()>;

    auto on(TSuite suite) {
        suites.emplace_back(suite);
    }

    template <class... Ts>
    void on(events::test<Ts...> test) {

        auto execute = std::empty(test.tag);
        for(const auto& tag_element: test.tag) {
            if(utility::is_match(tag_element, "skip")) {
                on(events::skip<>{.type = test.type, .name = test.name});
                return;
            }
        }

        auto filter = [](auto...) {
            return true;
        };

        /// reporter.on(events::test_run{.type = test.type, .name = test.name});

        reporter.on(
            events::test_begin{.type = test.type, .name = test.name, .location = test.location});

        test();

        reporter.on(events::test_end{.type = test.type, .name = test.name});
    }

    template <class... Ts>
    void on(events::skip<Ts...> test) {
        reporter.on(events::test_skip{.type = test.type, .name = test.name});
    }

    template <class TExpr>
    [[nodiscard]] bool on(events::assertion<TExpr> assertion) {
        if(static_cast<bool>(assertion.expr)) {
            reporter.on(events::assertion_pass<TExpr>{.expr = assertion.expr,
                                                      .location = assertion.location});
            return true;
        }

        ++fails;
        reporter.on(
            events::assertion_fail<TExpr>{.expr = assertion.expr, .location = assertion.location});
        return false;
    }

    void on(events::fatal_assertion fatal_assertion) {
        reporter.on(fatal_assertion);
    }

    template <class TMsg>
    void on(events::log<TMsg> l) {
        reporter.on(l);
    }

    [[nodiscard]] auto run(run_cfg rc = {}) -> bool {
        run_ = true;
        reporter.on(events::run_begin{.argc = rc.argc, .argv = rc.argv});
        for(const auto& [suite, suite_name]: suites) {
            // add reporter in/out
            /// TODO: Add test suite start

            suite();

            /// TODO: Add test suite end.
        }

        suites.clear();

        if(rc.report_errors) {
            report_summary();
        }

        return fails > 0;
    }

    auto report_summary() -> void {
        if(static auto once = true; once) {
            once = false;
            reporter.on(events::summary{});
        }
    }

protected:
    bool run_{};
    reporter<printer> reporter{};
    std::vector<TSuite> suites{};
    std::size_t fails{};
};

}  // namespace cfg

// template <>
// auto ut::cfg<ut::override> = cfg::runner{};

int main(int argc, const char* argv[]) {
    using namespace clice;

    /// return ut::cfg<>.run({.argc = argc, .argv = argv});

    llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);
    llvm::cl::ParseCommandLineOptions(argc, argv, "clice test\n");

    if(!cl::test_filter.empty()) {
        if(auto pattern = GlobPattern::create(cl::test_filter)) {
            cl::filter_pattern = std::move(*pattern);
        } else {
            llvm::outs() << "Invaild pattern: {}" << cl::test_filter << "\n";
        }
    }

    if(!cl::resource_dir.empty()) {
        fs::resource_dir = cl::resource_dir.getValue();
    } else {
        if(auto result = fs::init_resource_dir(argv[0]); !result) {
            llvm::outs() << std::format("Failed to get resource directory, because {}\n",
                                        result.error());
            return 1;
        }
    }

    return ut::cfg<>.run();
}
