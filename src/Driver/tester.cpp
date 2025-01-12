#include "Server/Async.h"
#include "llvm/Support/CommandLine.h"

#include "Support/Support.h"

using namespace clice;

namespace cl {

llvm::cl::opt<std::string> execute("execute", llvm::cl::desc("The execute path"));
llvm::cl::opt<std::string> source("execute", llvm::cl::desc("The test source path"));

}  // namespace cl

int main() {
    async::spawn(
        [](json::Value value) -> async::Task<> {
            print("Receive: {0}", value);
            co_return;
        },
        cl::execute.getValue(),
        {cl::source.getValue()});

    return 0;
}
