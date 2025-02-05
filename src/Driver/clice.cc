#include "Support/Logger.h"
#include "Server/Server.h"
#include "llvm/Support/CommandLine.h"

using namespace clice;

namespace cl {

llvm::cl::opt<std::string> config("config",
                                  llvm::cl::desc("The path of the config file"),
                                  llvm::cl::value_desc("path"));

llvm::cl::opt<bool> pipe("pipe", llvm::cl::desc("Use pipe mode"));

llvm::cl::opt<std::string> resource_dir("resource-dir", llvm::cl::desc("Resource dir path"));

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
        config::load(argv[0], cl::config.getValue());
        log::info("Successfully loaded configuration file from {0}.", cl::config.getValue());
    }

    /// Get the resource directory.
    if(!cl::resource_dir.empty()) {
        fs::resource_dir = cl::resource_dir.getValue();
    } else {
        if(auto error = fs::init_resource_dir(argv[0])) {
            log::fatal("Failed to get resource directory, because {0}", error);
            return 1;
        }
    }

    static Server server;
    auto loop = [](json::Value value) -> async::Task<> {
        co_await server.onReceive(value);
    };

    if(cl::pipe && cl::pipe.getValue()) {
        async::net::listen(loop);
    } else {
        async::net::listen("127.0.0.1", 50051, loop);
    }

    async::run();
}

