#include "Test/Test.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/ADT/SmallString.h"
#include "Support/Support.h"

namespace clice {

namespace cl {

llvm::cl::opt<std::string> test_dir("test-dir",
                                    llvm::cl::desc("specify the test source directory path"),
                                    llvm::cl::value_desc("path"),
                                    llvm::cl::Required);

llvm::cl::opt<std::string> resource_dir("resource-dir", llvm::cl::desc("Resource dir path"));

}  // namespace cl

namespace test {

llvm::StringRef source_dir() {
    return cl::test_dir.getValue();
}

llvm::StringRef resource_dir() {
    return cl::resource_dir.c_str();
}

}  // namespace test

}  // namespace clice

int main(int argc, char** argv) {
    using namespace clice;

    testing::InitGoogleTest(&argc, argv);
    llvm::cl::ParseCommandLineOptions(argc, argv, "clice test\n");

    if(!cl::resource_dir.empty()) {
        fs::resource_dir = cl::resource_dir.getValue();
    } else {
        if(auto error = fs::init_resource_dir(argv[0])) {
            llvm::outs() << std::format("Failed to get resource directory, because {}\n", error);
            return 1;
        }
    }

    return RUN_ALL_TESTS();
}

