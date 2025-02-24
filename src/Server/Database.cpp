#include "Server/Database.h"
#include "Compiler/Compilation.h"
#include "Support/Logger.h"
#include "Support/JSON.h"

namespace clice {

/// Update the compile commands with the given file.
void CompilationDatabase::updateCommands(llvm::StringRef filename) {
    auto path = path::real_path(filename);
    filename = path;

    /// Read the compile commands from the file.
    json::Value json = nullptr;

    if(auto buffer = llvm::MemoryBuffer::getFile(filename)) {
        auto content = buffer.get()->getBuffer();
        if(auto res = parse(content); res.has_value()) {
            commands = std::move(res).value();
        } else {
            log::warn("Failed to parse file {0}, reason: {1}", filename, res.error());
            return;
        }
    } else {
        log::warn("Failed to read file {0}", filename);
        return;
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

namespace {

struct SkipState {
    bool skipCurrent = false;
    bool skipNext = false;
};

/// TODO:
/// Add more cases in compile_commands to skip.
SkipState shouldSkip(llvm::StringRef argument, SkipState old) {
    constexpr const char* skipTwice[] = {"-o"};
    if(std::any_of(std::begin(skipTwice), std::end(skipTwice), [&](llvm::StringRef c) {
           return argument == c;
       })) {
        return {true, true};
    }

    constexpr const char* skipCurrent[] = {"-g", "-O0", "-O1", "-O2", "-O3", "-Os", "-Oz"};
    if(std::ranges::any_of(skipCurrent, [&](llvm::StringRef c) { return argument == c; })) {
        return {true, false};
    }

    return {false, old.skipNext};
}

struct CompileCommand {
    /// Absolute path of the file.
    std::string file;

    /// The compile command.
    std::string command;
};

void joinCompileCommands(std::string& out, llvm::StringRef arg, llvm::StringRef directory) {
    const llvm::StringRef platSeparator = path::get_separator();

    // It looks like a relative path, join the directory.
    if(!arg.starts_with('-') && arg.slice(1, llvm::StringRef::npos).contains(platSeparator) &&
       path::is_relative(arg)) {
        out += path::join(directory, arg);
    } else {
        out += arg;
    }
    out += ' ';
}

std::expected<CompileCommand, std::string> parseArgumentsStyle(const json::Object* object,
                                                               const json::Array* args) {
    CompileCommand cmd;
    auto directory = object->getString("directory");
    auto file = object->getString("file");
    if(file.has_value() && directory.has_value()) {
        cmd.file = path::join(directory.value(), file.value());
    } else {
        return std::unexpected("Json item doesn't have a \"file\" or \"directory\" key");
    }

    for(SkipState state; auto& buildArg: *args) {
        assert(buildArg.getAsString().has_value() && "\"arguments\" must be a list of string.");

        llvm::StringRef arg = buildArg.getAsString().value();
        if(state = shouldSkip(arg, state); state.skipCurrent) {
            state.skipCurrent = false;
            continue;
        } else if(state.skipNext) {
            state.skipNext = false;
            continue;
        }

        // xmake will generate arguments like "-I.", "-Iinclude", which is a relative path.
        if(arg.starts_with("-I") && arg.size() > 2) {
            auto absPath =
                std::format("-I{}{}{}", *directory, path::get_separator(), arg.drop_front(2));
            joinCompileCommands(cmd.command, absPath, *directory);
        } else {
            joinCompileCommands(cmd.command, arg, *directory);
        }
    }

    cmd.command.shrink_to_fit();
    return cmd;
}

std::expected<CompileCommand, std::string> parseCommandStyle(const json::Object* object,
                                                             llvm::StringRef command) {
    CompileCommand cmd;
    auto directory = object->getString("directory");
    auto file = object->getString("file");
    if(file.has_value() && directory.has_value()) {
        if(path::is_absolute(file.value())) {
            cmd.file = file.value();
        } else {
            cmd.file = path::join(directory.value(), file.value());
        }
    } else {
        return std::unexpected("Json item doesn't have a \"file\" key");
    }

    SkipState state;
    llvm::StringRef line = command.ltrim();
    while(!line.empty()) {
        auto [arg, remain] = line.split(' ');
        if(state = shouldSkip(arg, state); state.skipCurrent) {
            state.skipCurrent = false;
            line = remain.ltrim();
            continue;
        } else if(state.skipNext) {
            state.skipNext = false;
            line = remain.ltrim();
            continue;
        }

        joinCompileCommands(cmd.command, arg, *directory);
        line = remain.ltrim();
    }

    cmd.command.shrink_to_fit();
    return cmd;
}

std::expected<CompileCommand, std::string> parseCompileCommand(const json::Object* object) {
    if(auto arguments = object->getArray("arguments")) {
        return parseArgumentsStyle(object, arguments);
    } else if(auto command = object->getString("command")) {
        return parseCommandStyle(object, *command);
    } else {
        return std::unexpected("Json item doesn't have a \"arguments\" or \"command\" key");
    }
}
}  // namespace

std::expected<llvm::StringMap<std::string>, std::string>
    CompilationDatabase::parse(llvm::StringRef content) {
    llvm::StringMap<std::string> commands;

    if(auto object = json::parse(content)) {
        auto items = object->getAsArray();
        assert(items && "json is not an array");

        for(auto& item: *items) {
            auto object = item.getAsObject();
            if(!object) {
                return std::unexpected(
                    std::format("Compilation Database requires an array of object, but got {0}",
                                refl::enum_name(item.kind())));
            }

            if(auto res = parseCompileCommand(object); res.has_value()) {
                auto [file, command] = std::move(res).value();
                commands[file] = std::move(command);
            } else {
                return std::unexpected(
                    std::format("Failed to parse CDB file, reason: {0}", res.error()));
            }
        }
    } else {
        return std::unexpected(
            std::format("Failed to parse json file, because {0}", object.takeError()));
    }

    return commands;
}

}  // namespace clice
