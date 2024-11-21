#include "Compiler/Command.h"
#include "clang/Tooling/DependencyScanning/DependencyScanningTool.h"

namespace clice {

namespace clice {}

CommandManager::CommandManager(llvm::StringRef path) {
    std::string error;
    CDB = clang::tooling::CompilationDatabase::loadFromDirectory(path, error);
    if(!CDB) {
        llvm::errs() << "Failed to load compilation database: " << error << "\n";
        return;
    }

    /// FIXME: module support
    // using namespace clang::tooling::dependencies;
    // DependencyScanningService service(ScanningMode::DependencyDirectivesScan,
    //                                   ScanningOutputFormat::P1689);
    // DependencyScanningTool tool(service);
    //
    // for(auto& command: CDB->getAllCompileCommands()) {
    //    auto rule = tool.getP1689ModuleDependencyFile(command, command.Directory);
    //    if(rule) {
    //        moduleMap[rule->Provides->ModuleName] = command.Filename;
    //    } else {
    //        llvm::errs() << std::format("Failed to scan module dependencies for {}, Because:
    //        {}\n",
    //                                    command.Filename,
    //                                    llvm::toString(rule.takeError()));
    //    }
    //}
}

llvm::ArrayRef<const char*> CommandManager::lookup(llvm::StringRef file) {
    auto iter = cache.find(file);
    if(iter != cache.end()) {
        return llvm::ArrayRef(args).slice(iter->second.index, iter->second.size);
    }

    auto commands = CDB->getCompileCommands(file);
    assert(commands.size() == 1);
    auto& command = commands[0];

    std::size_t index = args.size();
    for(auto& arg: command.CommandLine) {
        auto data = allocator.Allocate<char>(arg.size() + 1);
        std::copy(arg.begin(), arg.end(), data);
        data[arg.size()] = '\0';
        args.push_back(data);
    }

    cache.try_emplace(file, Data{index, args.size() - index});
    return llvm::ArrayRef(args).slice(index, args.size() - index);
}

}  // namespace clice
