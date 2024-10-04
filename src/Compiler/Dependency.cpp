#include <Compiler/Dependency.h>
#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/DependencyScanning/DependencyScanningTool.h>

namespace clice::dependencies {

namespace {

/// module name -> file path.
llvm::StringMap<StringRef> moduleMap;
llvm::BumpPtrAllocator allocator;

void scan() {
    using namespace clang::tooling::dependencies;
    DependencyScanningService service(ScanningMode::DependencyDirectivesScan, ScanningOutputFormat::P1689);
    // TODO: figure out vfs
    DependencyScanningTool tool(service);

    // TODO: figure out
    clang::tooling::CompileCommand command;
    auto rule = tool.getP1689ModuleDependencyFile(command, "CWD");
}

}  // namespace

void load(ArrayRef<StringRef> dirs) {
    for(auto dir: dirs) {

        std::string message;
        auto CDB = clang::tooling::CompilationDatabase::loadFromDirectory(dir, message);
        if(!CDB) {}
        // TODO:
        // remove unused compile commands.
        // scan the whole project(if it is module file scan it).
        // record include graph?? unsure
        // use BumpPtrAllocator to store the commands for
        // - reduce memory usage
        // - expose `std::vector<const char*>` to `Compiler::Invocation`
    }
}

}  // namespace clice::dependencies
