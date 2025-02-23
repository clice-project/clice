#include "Server/Database.h"
#include "Compiler/Compilation.h"
#include "Support/Logger.h"

namespace clice {

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

        if(auto res = parseCompileCommand(object); res.has_value()) {
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

const static llvm::StringRef platSeparator = path::get_separator();

void joinCompileCommands(std::string& out, llvm::StringRef arg, llvm::StringRef directory) {
    // It looks like a relative path, join the directory.
    if(!arg.starts_with('-') && arg.slice(1, llvm::StringRef::npos).contains(platSeparator) &&
       !path::is_absolute(arg)) {
        out += path::join(directory, arg);
    } else {
        out += arg;
    }
    out += ' ';
}

std::expected<CompileCommand, std::string> parseArgumentsStyle(const json::Object* object) {
    CompileCommand cmd;

    auto directory = object->getString("directory");
    auto file = object->getString("file");
    if(file.has_value() && directory.has_value()) {
        cmd.file = path::join(directory.value(), file.value());
    } else {
        return std::unexpected("Json item doesn't have a \"file\" or \"directory\" key");
    }

    if(auto args = object->getArray("arguments")) {
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

            joinCompileCommands(cmd.command, arg, *directory);
        }
    } else {
        return std::unexpected("Json item doesn't have a \"arguments\" key");
    }

    cmd.command.shrink_to_fit();
    return cmd;
}

std::expected<CompileCommand, std::string> parseCommandStyle(const json::Object* object) {
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

    if(auto command = object->getString("command")) {

        SkipState state;
        llvm::StringRef line = command.value().ltrim();
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

    } else {
        return std::unexpected("Json item doesn't have a \"command\" key");
    }

    return cmd;
}
}  // namespace

std::expected<CompileCommand, std::string> parseCompileCommand(const json::Object* object) {
    if(object->get("arguments")) {
        return parseArgumentsStyle(object);
    } else if(object->get("command")) {
        return parseCommandStyle(object);
    } else {
        return std::unexpected("Json item doesn't have a \"arguments\" or \"command\" key");
    }
}

}  // namespace clice
