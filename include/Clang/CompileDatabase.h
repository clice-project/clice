#pragma once

#include <Clang/Clang.h>

namespace clice {

class CompileDatabase {
private:
    std::unique_ptr<clang::tooling::CompilationDatabase> database;

public:
    static auto& instance() {
        static CompileDatabase instance;
        return instance;
    }

    void load(std::string_view path);

    auto lookup(std::string_view path) {
        auto commands = database->getCompileCommands(path);
        llvm::ArrayRef command = commands[0].CommandLine;
        std::vector<const char*> args = {command.front().c_str(), "-Xclang", "-no-round-trip-args"};
        for(auto& arg: command.drop_front()) {
            args.push_back(arg.c_str());
        }
        return args;
    }
};

}  // namespace clice
