#include "Test/Test.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Signals.h"

namespace clice {

namespace cl {

llvm::cl::opt<std::string> test_dir("test-dir",
                                    llvm::cl::desc("specify the test source directory path"),
                                    llvm::cl::value_desc("path"),
                                    llvm::cl::Required);

llvm::cl::opt<std::string> resource_dir("resource-dir", llvm::cl::desc("Resource dir path"));

}  // namespace cl

namespace testing {

llvm::StringRef test_dir() {
    return cl::test_dir;
}

}  // namespace testing

}  // namespace clice

int main(int argc, char** argv) {
    using namespace clice;

    llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);

    ::testing::InitGoogleTest(&argc, argv);
    llvm::cl::ParseCommandLineOptions(argc, argv, "clice test\n");

    if(!cl::resource_dir.empty()) {
        fs::resource_dir = cl::resource_dir.getValue();
    } else {
        if(auto result = fs::init_resource_dir(argv[0]); !result) {
            llvm::outs() << std::format("Failed to get resource directory, because {}\n",
                                        result.error());
            return 1;
        }
    }

    bool res = RUN_ALL_TESTS();
    return res;
}

