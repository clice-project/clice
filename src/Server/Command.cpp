#include <Server/Command.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

namespace clice {

std::vector<const char*> CompilationDatabase::lookup(clang::StringRef path) {
    auto& command = CDB->getCompileCommands(path).front();
    std::vector<const char*> args;
    for(auto& arg: command.CommandLine) {
        // TODO:
        // some modification
        args.push_back(arg.c_str());
    }
    return args;
}

void CompilationDatabase::load(clang::StringRef path) {
    std::string error;
    CDB = clang::tooling::CompilationDatabase::loadFromDirectory(path, error);
    if(!CDB) {
        spdlog::error("Failed to load compilation database: {}", error);
        spdlog::default_logger()->flush();
        std::terminate();
    }
}

}  // namespace clice
