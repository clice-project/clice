#include "Support/Logger.h"
#include "Compiler/Command.h"
#include "Compiler/Compilation.h"
#include "Support/FileSystem.h"

namespace clice {

std::expected<void, std::string> mangle_command(llvm::StringRef command,
                                               llvm::SmallVectorImpl<const char*>& out,
                                               llvm::SmallVectorImpl<char>& buffer) {
    llvm::SmallString<128> current;
    llvm::SmallVector<uint32_t> indices;
    bool inSingleQuote = false;
    bool inDoubleQuote = false;

    for(size_t i = 0; i < command.size(); ++i) {
        char c = command[i];
        if(c == ' ' && !inSingleQuote && !inDoubleQuote) {
            if(!current.empty()) {
                indices.push_back(buffer.size());
                buffer.append(current);
                buffer.push_back('\0');
                current.clear();
            }
        } else if(c == '\'' && !inDoubleQuote) {
            inSingleQuote = !inSingleQuote;
        } else if(c == '"' && !inSingleQuote) {
            inDoubleQuote = !inDoubleQuote;
        } else {
            current.push_back(c);
        }
    }

    if(!current.empty()) {
        indices.push_back(buffer.size());
        buffer.append(current);
        buffer.push_back('\0');
    }

    /// Add resource directory.
    indices.push_back(buffer.size());
    current = std::format("-resource-dir={}", fs::resource_dir);
    buffer.append(current);
    buffer.push_back('\0');

    /// FIXME: use better way to remove args.
    for(size_t i = 0; i < indices.size(); ++i) {
        llvm::StringRef arg(buffer.data() + indices[i]);

        /// Skip `-c` and `-o` arguments.
        if(arg == "-c") {
            continue;
        }

        if(arg.starts_with("-o")) {
            if(arg == "-o") {
                ++i;
            }
            continue;
        }

        if(arg.starts_with("@CMakeFiles")) {
            continue;
        }

        /// TODO: remove PCH.

        out.push_back(arg.data());
    }

    return {};
}

/// Update the compile commands with the given file.
void CompilationDatabase::updateCommands(llvm::StringRef filename) {
    auto path = path::real_path(filename);
    filename = path;

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
        log::warn(
            "Compilation CompilationDatabase requires a array of object, but get {0}, input file: {1}",
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
                "Compilation CompilationDatabase requires an array of object, but get a array of {0}, input file: {1}",
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
    // CompilationParams params;
    // for(auto& [path, command]: commands) {
    //     params.srcPath = path;
    //     params.command = command;
    //
    //    auto name = scanModuleName(params);
    //    if(!name.empty()) {
    //        moduleMap[name] = path;
    //    }
    //}

    log::info("Successfully built module map, total {0} modules", moduleMap.size());
}

void CompilationDatabase::updateCommand(llvm::StringRef file, llvm::StringRef command) {
    commands[path::real_path(file)] = command;
}

/// Update the module map with the given file and module name.
void CompilationDatabase::updateModule(llvm::StringRef file, llvm::StringRef name) {
    moduleMap[path::real_path(file)] = file;
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
