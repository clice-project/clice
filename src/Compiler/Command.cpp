#include "Support/Logger.h"
#include "Compiler/Command.h"
#include "Compiler/Compilation.h"
#include "Support/FileSystem.h"
#include "llvm/Support/CommandLine.h"

namespace clice {

/// Update the compile commands with the given file.
void CompilationDatabase::update_commands(this Self& self, llvm::StringRef filename) {
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

        self.add_command(path, *command);
    }

    log::info("Successfully loaded compile commands from {0}, total {1} commands",
              filename,
              self.commands.size());

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

    log::info("Successfully built module map, total {0} modules", self.moduleMap.size());
}

/// Update the module map with the given file and module name.
void CompilationDatabase::update_module(llvm::StringRef file, llvm::StringRef name) {
    moduleMap[path::real_path(file)] = file;
}

/// Lookup the module interface unit file path of the given module name.
llvm::StringRef CompilationDatabase::get_module_file(llvm::StringRef name) {
    auto iter = moduleMap.find(name);
    if(iter == moduleMap.end()) {
        return "";
    }
    return iter->second;
}

llvm::StringRef CompilationDatabase::save_string(this Self& self, llvm::StringRef string) {
    auto it = self.unique.find(string);

    /// FIXME: arg may be empty?

    /// If we already store the argument, reuse it.
    if(it != self.unique.end()) {
        return *it;
    }

    /// Allocate new argument.
    const auto size = string.size();
    auto ptr = self.memory_pool.Allocate<char>(size + 1);
    std::memcpy(ptr, string.data(), size);
    ptr[size] = '\0';

    /// Insert new argument.
    auto result = llvm::StringRef(ptr, size);
    self.unique.insert(result);
    return result;
}

std::vector<const char*> CompilationDatabase::save_args(this Self& self,
                                                        llvm::ArrayRef<const char*> args) {
    std::vector<const char*> result;
    result.reserve(args.size());

    for(auto i = 0; i < args.size(); i++) {
        result.emplace_back(self.save_string(args[i]).data());
    }

    return result;
}

void CompilationDatabase::add_command(this Self& self,
                                      llvm::StringRef path,
                                      llvm::StringRef command,
                                      Style style) {
    llvm::SmallVector<const char*> args;

    /// temporary allocator to meet the argument requirements of tokenize.
    llvm::BumpPtrAllocator allocator;
    llvm::StringSaver saver(allocator);

    /// FIXME: we may want to check the first argument of command to
    /// make sure its mode.
    if(style == Style::GNU) {
        llvm::cl::TokenizeGNUCommandLine(command, saver, args);
    } else if(style == Style::MSVC) {
        llvm::cl::TokenizeWindowsCommandLineFull(command, saver, args);
    } else {
        std::abort();
    }

    auto path_ = self.save_string(path);
    auto new_args = self.save_args(args);

    auto it = self.commands.find(path_.data());
    if(it == self.commands.end()) {
        self.commands.try_emplace(path_.data(),
                                  std::make_unique<std::vector<const char*>>(std::move(new_args)));
    } else {
        *it->second = std::move(new_args);
    }
}

llvm::ArrayRef<const char*> CompilationDatabase::get_command(this Self& self,
                                                             llvm::StringRef path) {
    auto path_ = self.save_string(path);
    auto it = self.commands.find(path_.data());
    if(it != self.commands.end()) {
        return *it->second;
    } else {
        return {};
    }
}

}  // namespace clice
