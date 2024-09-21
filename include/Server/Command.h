#pragma once

#include <clang/Tooling/CompilationDatabase.h>

namespace clice {

namespace command {

std::vector<const char*> decorate(const clang::tooling::CompileCommand& command);

class Command {
public:
    /// according to config, decorate the input command.
    /// e.g. add `resource-dir`, remove or transform unsupported flags like `/std:lastest`.
    Command(const clang::tooling::CompileCommand& command);

    Command& append(llvm::StringRef arg) {
        auto data = allocator.Allocate<char>(arg.size() + 1);
        if(!arg.empty()) {
            std::memcpy(data, arg.data(), arg.size());
        }
        data[arg.size()] = '\0';
        args.push_back(data);
        return *this;
    }

private:
    std::vector<const char*> args;
    llvm::BumpPtrAllocator allocator;
};

// TODO: compile database logic

}  // namespace command

}  // namespace clice
