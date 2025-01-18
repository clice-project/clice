#include "Server/Logger.h"
#include "Server/Database.h"
#include "Support/Support.h"
#include "Compiler/Compilation.h"

namespace clice {

/// Update the compile commands with the given file.
void CompilationDatabase::update(llvm::StringRef filename) {
    /// Read the compile commands from the file.
    json::Value json = nullptr;

    if(auto buffer = llvm::MemoryBuffer::getFile(filename)) {
        if(auto result = json::parse(buffer->get()->getBuffer())) {
            /// llvm::json::Value will hold on string buffer.
            /// Do not worry about the lifetime of the buffer.
            /// Release buffer to save memory.
            json = std::move(result.get());
        } else {
            log::warn("Failed to parse json file at {0}, because {1}",
                      filename,
                      result.takeError());
            return;
        }
    } else {
        log::warn("Failed to read file {0}", filename);
        return;
    }

    assert(json.kind() != json::Value::Null && "json is nullptr");

    if(json.kind() != json::Value::Array) {
        log::warn("Compilation Database requires a array of object, but get {0}, input file: {1}",
                  refl::enum_name(json.kind()),
                  filename);
        return;
    }

    auto elements = json.getAsArray();
    assert(elements && "json is not an array");

    for(auto& element: *elements) {
        auto object = element.getAsObject();
        if(!object) {
            log::warn(
                "Compilation Database requires an array of object, but get a array of {0}, input file: {1}",
                refl::enum_name(element.kind()),
                filename);
            continue;
        }

        /// FIXME: currently we assume all path here is absolute.
        /// Add `directory` field in the future.

        llvm::SmallString<128> path;

        if(auto file = object->getString("file")) {
            if(auto error = fs::real_path(*file, path)) {
                log::warn("Failed to get real path of {0}, because {1}", *file, error.message());
                continue;
            }
        } else {
            log::warn("The element does not have a file field, input file: {0}", filename);
            continue;
        }

        auto command = object->getString("command");
        if(!command) {
            log::warn("The key:{0} does not have a command field, input file: {1}", path, filename);
            continue;
        }

        commands[path] = *command;
    }

    log::info("Successfully loaded compile commands from {0}, total {1} commands",
              filename,
              commands.size());

    /// Scan all files to build module map.
    CompilationParams params;
    for(auto& [path, command]: commands) {
        params.srcPath = path;
        params.command = command;
        auto info = scanModule(params);
        if(!info) {
            log::warn("Failed to scan module from {0}, because {1}", path, info.takeError());
            continue;
        }

        if(info->isInterfaceUnit) {
            assert(!info->name.empty() && "module name is empty");
            moduleMap[info->name] = path;
        }
    }

    log::info("Successfully built module map, total {0} modules", moduleMap.size());
}

/// Update the module map with the given file and module name.
void CompilationDatabase::update(llvm::StringRef file, llvm::StringRef name) {
    moduleMap[name] = file;
}

/// Lookup the compile commands of the given file.
llvm::StringRef CompilationDatabase::getCommand(llvm::StringRef file) {
    auto iter = commands.find(file);
    if(iter == commands.end()) {
        return "";
    }
    return iter->second;
}

/// Lookup the module interface unit file path of the given module name.
llvm::StringRef CompilationDatabase::getModuleFile(llvm::StringRef name) {
    auto iter = moduleMap.find(name);
    if(iter == moduleMap.end()) {
        return "";
    }
    return iter->second;
}

}  // namespace clice
