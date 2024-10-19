#include <Test/Test.h>
#include <llvm/Support/CommandLine.h>

namespace clice {

llvm::cl::opt<std::string> test_dir_path("test-dir",
                                         llvm::cl::desc("specify the test source directory path"),
                                         llvm::cl::value_desc("path"),
                                         llvm::cl::Required);

std::string test_dir() {
    return test_dir_path;
}

}  // namespace clice

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    llvm::cl::ParseCommandLineOptions(argc, argv, "clice test\n");
    return RUN_ALL_TESTS();
}

