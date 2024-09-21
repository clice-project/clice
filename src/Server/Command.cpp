#include <Server/Logger.h>
#include <Server/Config.h>
#include <Server/Command.h>

namespace clice {

namespace command {

Command::Command(const clang::tooling::CompileCommand& command) {
    // FIXME:
    for(auto& arg: command.CommandLine) {
        append(arg);
    }

    append("-resource-dir");
    append(config::frontend().resource_dictionary);
}

}  // namespace command

}  // namespace clice
