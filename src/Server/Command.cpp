#include "Server/Command.h"
#include "Server/Config.h"
#include "Server/Logger.h"

namespace clice {

void CommandManager::update(llvm::StringRef dir) {
    llvm::SmallString<128> path;
    path::append(path, dir, "compile_commands.json");

    auto buffer = llvm::MemoryBuffer::getFile(path);
    if(!buffer) {
        log::warn("Failed to read compile_commands.json from {}, because {}",
                  dir,
                  buffer.getError().message());
        return;
    }

    auto json = json::parse(buffer.get()->getBuffer());
    if(!json) {
        log::warn("Failed to parse json file at {}, because {}", path, json.takeError());
        return;
    }

    if(!json->getAsArray()) {
        log::warn("Invalid JSON format at {}, Compilation Database requires Array, but get {}",
                  path,
                  refl::enum_name(json->kind()));
        return;
    }

    auto& CDB = CDBs[dir];

    /// Clear old commands.
    /// FIXME: it would be better to have cache in some way.
    CDB.clear();

    for(auto& value: *json->getAsArray()) {
        auto element = value.getAsObject();
        if(!element) {
            log::warn("Invalid JSON format at {}, Compilation Database requires Object, but get {}",
                      path,
                      refl::enum_name(value.kind()));
            continue;
        }

        /// Source file path.
        llvm::SmallString<128> path;

        auto file = element->getString("file");
        if(!file) {
            log::warn("the element in {} does not have a file field",
                      path,
                      refl::enum_name(value.kind()));
            continue;
        }

        if(path::is_relative(file.value())) {
            auto working = element->getString("directory");
            if(!working) {
                log::warn(
                    "Invalid JSON format at {}, {} is relative path, but directory is not provided",
                    path,
                    file.value());
            }

            path::append(path, working.value(), file.value());
        } else {
            path::append(path, file.value());
        }

        /// Command to compile the source file.
        llvm::SmallString<1024> rawCommand;

        if(auto command = element->getString("command")) {
            rawCommand = command.value();
        } else if(auto arguments = element->getArray("arguments")) {
            /// FIXME:
            std::terminate();
        } else {
            log::warn(
                "Invalid JSON format, the element in {} does not have a command or arguments field",
                path,
                refl::enum_name(value.kind()));
            continue;
        }

        CDB[path].emplace_back(rawCommand.str());
    }

    log::info("Compilation Database at {} is up-to-date", dir);
}

llvm::StringRef CommandManager::lookupFirst(llvm::StringRef file) {
    for(auto& [dir, cdb]: CDBs) {
        auto iter = cdb.find(file);
        if(iter != cdb.end()) {
            return iter->second.front();
        }
    }

    return {};
}

llvm::ArrayRef<std::string> CommandManager::lookup(llvm::StringRef file) {
    for(auto& [dir, cdb]: CDBs) {
        /// FIXME: currently we directly return the first match.
        /// it is better to provide a callback to handle all matches.
        auto iter = cdb.find(file);
        if(iter != cdb.end()) {
            return iter->second;
        }
    }

    return {};
}

}  // namespace clice
