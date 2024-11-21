#include "Basic/URI.h"
#include "Server/Server.h"
#include "llvm/Support/CommandLine.h"

using namespace clice;

int main(int argc, const char** argv) {
    llvm::cl::opt<std::string> config("config");
    llvm::cl::ParseCommandLineOptions(argc, argv);

    if(!config.hasArgStr()) {
        llvm::errs() << "Missing config file.\n";
        std::terminate();
    }

    llvm::outs() << "Config file: " << config << "\n";

    config::parse(argv[0], config);

    return 0;
}


