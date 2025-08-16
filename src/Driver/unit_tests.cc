#include "Test/Test.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Signals.h"
#include "Support/GlobPattern.h"
#include <print>

using namespace clice;
using namespace clice::testing;

namespace {

namespace cl = llvm::cl;

cl::opt<std::string> test_dir("test-dir",
                              cl::desc("specify the test source directory path"),
                              cl::value_desc("path"),
                              cl::Required);

cl::opt<std::string> resource_dir("resource-dir", cl::desc("Resource dir path"));

cl::opt<std::string> test_filter("test_filter");

/// A string to hold output....
std::string output_buffer;

std::optional<GlobPattern> pattern;

}  // namespace

namespace clice::testing {

Runner& Runner::instance() {
    static Runner runner;
    return runner;
}

void Runner::add_suite(std::string_view name, Suite suite) {
    suites[name].emplace_back(suite);
}

void Runner::run_test(std::string_view name, Test test) {
    std::string full_name = std::format("{}.{}", curr_suite_name, name);

    /// If this test if filter, directly return.
    if(!pattern || !pattern->match(full_name)) {
        curr_tests_count += 1;
        curr_filtered_tests_count += 1;
        return;
    }

    failed = false;

    using namespace std::chrono;

    std::format_to(std::back_inserter(output_buffer),
                   "\033[32m[ RUN      ] {}.{}\033[0m\n",
                   curr_suite_name,
                   name);
    auto begin = system_clock::now();

    test();

    auto duration = duration_cast<milliseconds>(system_clock::now() - begin);
    std::format_to(std::back_inserter(output_buffer),
                   "\033[32m[   {} ] {}.{} ({} ms)\033[0m\n",
                   failed ? "FAILED" : "    OK",
                   curr_suite_name,
                   name,
                   duration.count());

    /// Update test information.
    curr_tests_count += 1;
    total_tests_count += 1;

    curr_test_duration += duration;
    totol_test_duration += duration;
}

void Runner::fail(std::string expression, std::source_location location) {
    failed = true;
}

int Runner::run_tests() {
    /// Register all tests.
    std::println("\033[32m[----------] Global test environment set-up.\033[0m");

    for(auto& [suite_name, suite]: suites) {
        output_buffer.clear();

        curr_suite_name = suite_name;
        curr_tests_count = 0;
        curr_filtered_tests_count = 0;
        curr_test_duration = std::chrono::milliseconds();

        std::format_to(std::back_inserter(output_buffer),
                       "\033[32m[----------] tests from {}\033[0m\n",
                       suite_name);

        for(auto& callback: suite) {
            callback();
        }

        std::format_to(std::back_inserter(output_buffer),
                       "\033[32m[----------] {} tests from {} ({} ms total)\033[0m\n\n",
                       total_tests_count,
                       suite_name,
                       totol_test_duration.count());

        /// If all tests in this suite case are filtered, we skip output of it.
        if(curr_filtered_tests_count != curr_tests_count) {
            std::print("{}", output_buffer);
        }
    }

    std::println("\033[32m[----------] Global test environment tear-down\033[0m");
    std::println("\033[32m[==========] {} tests from {} test suites ran. ({} ms total)\033[0m",
                 total_tests_count,
                 suites.size(),
                 totol_test_duration.count());

    return 0;
}

}  // namespace clice::testing

int main(int argc, const char* argv[]) {
    llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);
    llvm::cl::ParseCommandLineOptions(argc, argv, "clice test\n");

    if(!test_filter.empty()) {
        if(auto result = GlobPattern::create(test_filter)) {
            pattern.emplace(std::move(*result));
        }
    }

    if(resource_dir.empty()) {
        fs::resource_dir = resource_dir;
    } else {
        if(auto result = fs::init_resource_dir(argv[0]); !result) {
            std::println("Failed to get resource directory, because {}", result.error());
            return 1;
        }
    }

    using namespace clice::testing;
    return Runner::instance().run_tests();
}
