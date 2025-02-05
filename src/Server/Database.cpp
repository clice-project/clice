#include "Support/Logger.h"
#include "Server/Database.h"
#include "Support/FileSystem.h"
#include "Compiler/Compilation.h"

#include <expected>

namespace clice {

namespace {

struct CompileCommand {
    /// Absolute path of the file.
    std::string file;

    /// The compile command.
    std::string command;
};

/// Try extract compile command from an item in CDB file. An reason will be returned if failed.
std::expected<CompileCommand, std::string> tryParseCompileCommand(const json::Object* object) {
    llvm::SmallString<128> path, buffer;
    if(auto dir = object->getString("directory"))
        buffer = dir.value();

    if(auto file = object->getString("file")) {
        buffer = path::join(buffer, *file);
        if(auto error = fs::real_path(buffer, path)) {
            auto reason =
                std::format("Failed to get realpath of {0}, because {1}", *file, error.message());
            return std::unexpected(std::move(reason));
        }
    } else {
        return std::unexpected(std::format("Json item doesn't have a \"file\" key"));
    }

    CompileCommand cmd;
    cmd.file = path.str();

    if(auto command = object->getString("command")) {
        cmd.command = command->str();
        return cmd;
    }

    if(auto args = object->getArray("arguments")) {
        for(auto& arg: *args) {
            cmd.command += *arg.getAsString(), cmd.command += ' ';
        }
        cmd.command.shrink_to_fit();
        return cmd;
    }

    auto reason = std::format("File:{0} doesn't have a \"command\" or \"arguments\" key.", path);
    return std::unexpected(std::move(reason));
}

}  // namespace

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

        if(auto res = tryParseCompileCommand(object); res.has_value()) {
            auto [file, command] = std::move(res).value();
            commands[file] = std::move(command);
        } else {
            log::warn("Failed to parse CDB file: {0}, reason: {1}", filename, res.error());
        }
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
