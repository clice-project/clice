#include "Server/Command.h"
#include "Server/Config.h"
#include "Server/Logger.h"

namespace clice {

void CommandManager::update(llvm::StringRef dir) {
    std::string error;
    auto CDB = clang::tooling::CompilationDatabase::loadFromDirectory(dir, error);
    if(!CDB) {
        log::fatal("Failed to load compilation database from {0}, because {1}", dir, error);
        return;
    }

    CDBs.try_emplace(dir, std::move(CDB));
    log::info("Successfully loaded compilation database from {0}.", dir);
}

std::vector<std::string> CommandManager::lookup(llvm::StringRef file) {
    std::vector<std::string> result;
    for(const auto& [dir, CDB]: CDBs) {
        auto commands = CDB->getCompileCommands(file);
        if(commands.empty()) {
            continue;
        }

        auto& args = commands[0].CommandLine;
        for(auto iter = args.begin(); iter != args.end(); ++iter) {
            if(iter->starts_with("-o")) {
                iter++;
                continue;
            } else if(iter->starts_with("-c")) {
                continue;
            } else if(*iter == "--") {
                continue;
            }

            result.push_back(std::move(*iter));
        }
        break;
    }

    result.push_back("-resource-dir=");
    result.back() += config::frontend().resource_dictionary;

    return result;
}

}  // namespace clice
