#include "Support/Logger.h"
#include "Server/Server.h"
#include "Support/Format.h"

#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/CommandLine.h"

namespace cl = llvm::cl;

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

int checkArguments(int argc, const char** argv) {
    cl::SetVersionPrinter(printVersion);
    cl::ParseCommandLineOptions(argc,
                                argv,
                                "clice is a new generation of language server for C/C++");

    for(int i = 0; i < argc; ++i) {
        clice::log::info("argv[{0}] = {1}", i, argv[i]);
    }

    if(::config_path.empty()) {
        clice::log::info("No config file specified; using default configuration.");
    } else {
        clice::config::load(argv[0], config_path.getValue());
        clice::log::info("Successfully loaded configuration file from {0}.",
                         config_path.getValue());
    }

    /// Get the resource directory.
    if(!resource_dir.empty()) {
        clice::fs::resource_dir = resource_dir.getValue();
    } else {
        clice::log::info("No resource directory specified; using default resource directory.");
        if(auto result = clice::fs::init_resource_dir(argv[0]); !result) {
            clice::log::warn("Cannot find default resource directory, because {}", result.error());
            return -1;
        }
    }

    return 0;
}

}  // namespace

int main(int argc, const char** argv) {
    llvm::InitLLVM guard(argc, argv);
    llvm::setBugReportMsg(
        "Please report bugs to https://github.com/clice-project/clice/issues and include the crash backtrace");

    /// Hide unrelated options.
    cl::HideUnrelatedOptions(category);

    if(int error = checkArguments(argc, argv); error != 0) {
        return error;
    }

    // static clice::Server server;
    // auto loop = [](json::Value value) -> async::Task<> {
    //     co_await server.onReceive(value);
    // };
    //
    // async::init();
    //
    // if(mode == "pipe") {
    //    async::net::listen(loop);
    //    clice::log::info("Server starts listening on stdin/stdout");
    //} else if(mode == "socket") {
    //    async::net::listen("127.0.0.1", 50051, loop);
    //    clice::log::info("Server starts listening on {}:{}", "127.0.0.1", 50051);
    //}
    //
    // async::run();
}

