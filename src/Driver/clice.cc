#include "Server/Logger.h"
#include "Server/Server.h"
#include "llvm/Support/CommandLine.h"

using namespace clice;

namespace cl {

llvm::cl::opt<std::string> config("config",
                                  llvm::cl::desc("The path of the config file"),
                                  llvm::cl::value_desc("path"));

llvm::cl::opt<bool> pipe("pipe", llvm::cl::desc("Use pipe mode"));

}  // namespace cl

int main(int argc, const char** argv) {
    for(int i = 0; i < argc; ++i) {
        log::warn("argv[{0}] = {1}", i, argv[i]);
    }

    llvm::cl::SetVersionPrinter([](llvm::raw_ostream& os) { os << "clice version: 0.0.1\n"; });
    llvm::cl::ParseCommandLineOptions(argc, argv, "clice language server");

    if(cl::config.empty()) {
        log::warn("No config file specified; using default configuration.");
    } else {
        /// config::load(argv[0], cl::config.getValue());
        log::info("Successfully loaded configuration file from {0}.", cl::config.getValue());
    }

    /// Get the resource directory.
    if(auto error = fs::init_resource_dir(argv[0])) {
        log::fatal("Failed to get resource directory, because {0}", error);
        return 1;
    }

    static Server server;
    auto loop = [](json::Value value) -> async::Task<> {
        co_await server.onReceive(value);
    };

    if(cl::pipe && cl::pipe.getValue()) {
        async::listen(loop);
    } else {
        async::listen(loop, "127.0.0.1", 50051);
    }

    async::run();
}

