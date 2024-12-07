#include "Test.h"
#include <llvm/Support/CommandLine.h>

namespace clice {

namespace cl {

llvm::cl::opt<std::string> test_dir("test-dir",
                                    llvm::cl::desc("specify the test source directory path"),
                                    llvm::cl::value_desc("path"),
                                    llvm::cl::Required);

llvm::SmallString<128> resource_dir;

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
    testing::InitGoogleTest(&argc, argv);
    llvm::cl::ParseCommandLineOptions(argc, argv, "clice test\n");

    if(auto error = clice::fs::real_path(llvm::Twine(argv[0]) + "/../../lib/clang/20)",
                                         clice::cl::resource_dir)) {
        llvm::errs() << "Failed to get resource directory, because " << error.message() << '\n';
    }

    return RUN_ALL_TESTS();
}

