#include "Test/Test.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Signals.h"
#include "Support/GlobPattern.h"
#include <print>

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

Runner& Runner::instance() {
    static Runner runner;
    return runner;
}

void Runner::add_suite(std::string_view name, Suite suite) {
    suites[name].emplace_back(suite);
}

void Runner::run_test(std::string_view name, Test test) {
    using namespace std::chrono;

    failed = false;

    std::println("\033[32m[ RUN      ] {}.{}\033[0m", curr_suite_name, name);
    auto begin = system_clock::now();

    test();

    auto duration = duration_cast<milliseconds>(system_clock::now() - begin);
    std::println("\033[32m[   {} ] {}.{} ({} ms)\033[0m",
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
    println("\033[32m[----------] Global test environment set-up.\033[0m");

    for(auto& [suite_name, suite]: suites) {
        curr_suite_name = suite_name;
        curr_tests_count = 0;
        curr_test_duration = std::chrono::milliseconds();

        std::println("\033[32m[----------] tests from {}\033[0m", suite_name);
        for(auto& callback: suite) {
            callback();
        }
        std::println("\033[32m[----------] {} tests from {} ({} ms total)\033[0m\n",
                     total_tests_count,
                     suite_name,
                     totol_test_duration.count());
    }

    println("\033[32m[----------] Global test environment tear-down\033[0m");
    println("\033[32m[==========] {} tests from {} test suites ran. ({} ms total)\033[0m",
            total_tests_count,
            suites.size(),
            totol_test_duration.count());

    return 0;
}

}  // namespace testing

}  // namespace clice

int main(int argc, const char* argv[]) {
    using namespace clice;

    // llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);
    // llvm::cl::ParseCommandLineOptions(argc, argv, "clice test\n");
    //
    // if(!cl::test_filter.empty()) {
    //    if(auto pattern = GlobPattern::create(cl::test_filter)) {
    //        cl::filter_pattern = std::move(*pattern);
    //    } else {
    //        llvm::outs() << "Invaild pattern: {}" << cl::test_filter << "\n";
    //    }
    //}
    //
    // if(!cl::resource_dir.empty()) {
    //    fs::resource_dir = cl::resource_dir.getValue();
    //} else {
    //    if(auto result = fs::init_resource_dir(argv[0]); !result) {
    //        llvm::outs() << std::format("Failed to get resource directory, because {}\n",
    //                                    result.error());
    //        return 1;
    //    }
    //}
    //

    return testing::Runner::instance().run_tests();
}
