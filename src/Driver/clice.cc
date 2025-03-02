#include "Server/Server.h"
#include "Support/Logger.h"
#include "Support/Format.h"

#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/CommandLine.h"

namespace cl = llvm::cl;
using namespace clice;

namespace {

static cl::OptionCategory category("clice options");

cl::opt<std::string>
    mode("mode",
         cl::cat(category),
         cl::value_desc("pipe|socket|indexer"),
         cl::desc("The mode of clice, default is pipe, socket is usually used for debugging"));

cl::opt<std::string> config_path(
    "config",
    cl::cat(category),
    cl::value_desc("path"),
    cl::desc(
        "The path of the clice config file, if not specified, the default config will be used"));

cl::opt<std::string> resource_dir(
    "resource-dir",
    cl::cat(category),
    cl::value_desc("path"),
    cl::desc(R"(The path of the clang resource directory, default is "../../lib/clang/version")"));

void printVersion(llvm::raw_ostream& os) {
    os << std::format("clice version: {}\n", clice::config::version)
       << std::format("llvm version: {}\n", clice::config::llvm_version);
}

/// Check the command line arguments and initialize the clice.
bool checkArguments(int argc, const char** argv) {
    /// Hide unrelated options.
    cl::HideUnrelatedOptions(category);

    // Set version printer and parse command line options
    cl::SetVersionPrinter(printVersion);
    cl::ParseCommandLineOptions(argc,
                                argv,
                                "clice is a new generation of language server for C/C++");

    for(int i = 0; i < argc; ++i) {
        log::info("argv[{}] = {}", i, argv[i]);
    }

    // Handle configuration file loading
    if(config_path.empty()) {
        log::info("No configuration file specified, using default settings");
    } else {
        llvm::StringRef path = config_path;
        // Try to load the configuration file and check the result
        if(auto result = config::load(argv[0], path); result) {
            log::info("Configuration file loaded successfully from: {}", path);
        } else {
            log::warn("Failed to load configuration file from: {} because {}",
                      path,
                      result.error());
            return false;
        }
    }

    // Initialize resource directory
    if(resource_dir.empty()) {
        log::info("No resource directory specified, using default resource directory");
        // Try to initialize default resource directory
        if(auto result = fs::init_resource_dir(argv[0]); !result) {
            log::warn("Cannot find default resource directory, because {}", result.error());
            return false;
        }
    } else {
        // Set and check the specified resource directory
        fs::resource_dir = resource_dir.getValue();
        if(fs::exists(fs::resource_dir)) {
            log::info("Resource directory found: {}", fs::resource_dir);
        } else {
            log::warn("Resource directory not found: {}", fs::resource_dir);
            return false;
        }
    }

    return true;
}

}  // namespace

/// The global server instance.
static Server instance;

int main(int argc, const char** argv) {
    llvm::InitLLVM guard(argc, argv);
    llvm::setBugReportMsg(
        "Please report bugs to https://github.com/clice-project/clice/issues and include the crash backtrace");

    if(!checkArguments(argc, argv)) {
        return 1;
    }

    async::init();

    auto loop = [&](json::Value value) -> async::Task<> {
        co_await instance.onReceive(value);
    };

    if(mode == "pipe") {
        async::net::listen(loop);
        log::info("Server starts listening on stdin/stdout");
    } else if(mode == "socket") {
        async::net::listen("127.0.0.1", 50051, loop);
        log::info("Server starts listening on {}:{}", "127.0.0.1", 50051);
    } else if(mode == "indexer") {
        /// TODO:
    } else {
        log::fatal("Invalid mode: {}", mode.getValue());
        return 1;
    }

    async::run();
}

