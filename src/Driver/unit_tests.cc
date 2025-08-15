#include "Test/Test.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Signals.h"
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

auto& suites() {
    static std::unordered_map<std::string_view, std::vector<TSuite>> instance;
    return instance;
}

void add_suite(std::string_view name, TSuite suite) {
    suites()[name].emplace_back(suite);
}

}  // namespace testing

}  // namespace clice

int main(int argc, const char* argv[]) {
    using namespace clice;

    for(auto& [name, suite]: testing::suites()) {
        clice::println("{} ", name);

        for(auto& fn: suite) {
            fn();
        }
    }

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
}
