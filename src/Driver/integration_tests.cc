#include "Async/Async.h"
#include "Support/Logger.h"
#include "Support/Support.h"
#include "llvm/Support/CommandLine.h"

using namespace clice;

namespace cl {

llvm::cl::opt<std::string> execute("execute", llvm::cl::desc("The execute path"));
llvm::cl::opt<std::string> source("source", llvm::cl::desc("The test source path"));

}  // namespace cl

static uint32_t id = 0;

async::Task<int> request(llvm::StringRef dir, llvm::StringRef file, llvm::StringRef method) {
    llvm::SmallString<128> path;
    path::append(path, cl::source, dir, file);

    auto content = llvm::MemoryBuffer::getFile(path);
    if(!content) {
        log::fatal("Failed to read file: {0}", path);
    }

    auto params = json::parse(content.get()->getBuffer());
    if(!params) {
        log::fatal("Failed to parse file: {0}", path);
    }

    json::Object request = json::Object{
        {"jsonrpc", "2.0"             },
        {"id",      id++              },
        {"method",  method            },
        {"params",  std::move(*params)},
    };

    log::info("Send Request: {0}", method);

    co_await async::net::write(std::move(request));

    co_return 1;
}

int main(int argc, const char** argv) {
    llvm::cl::SetVersionPrinter([](llvm::raw_ostream& os) { os << "clice version: 0.0.1\n"; });
    llvm::cl::ParseCommandLineOptions(argc, argv, "clice language server");

    async::net::spawn(cl::execute.getValue(),
                      {"--pipe=true", "--config=/home/ykiko/C++/clice2/docs/clice.toml"},
                      [](json::Value value) -> async::Task<> {
                          print("Receive: {0}", value);
                          co_return;
                      });

    auto p = request("initialize", "input.json", "initialize");
    async::run(p);
    p.release();
    return 0;
}
