#include "../Test.h"
#include <Compiler/Compiler.h>
#include <clang/Tooling/DependencyScanning/DependencyScanningTool.h>

namespace {

using namespace clice;

TEST(clice, ModuleScanner) {

    using namespace clang::tooling::dependencies;

    DependencyScanningService service(ScanningMode::DependencyDirectivesScan, ScanningOutputFormat::P1689);
    DependencyScanningTool tool(service);

    foreachFile("ModuleScanner", [&](std::string file, llvm::StringRef content) {
        clang::tooling::CompileCommand command;
        command.Filename = file;
        command.CommandLine = {"clang++", "-std=c++20", file};
        auto rule = tool.getP1689ModuleDependencyFile(command, "CWD");

        if(rule) {
            llvm::outs() << "Module: " << rule->Provides->ModuleName << "\n";
            llvm::outs() << "Module: " << rule->Provides->IsStdCXXModuleInterface << "\n";
            for(auto& info: rule->Requires) {
                llvm::outs() << info.ModuleName << " -> " << info.SourcePath << "\n";
            }
        }
    });
}

TEST(clice, Compiler) {
    const char* code = R"(
#include <cstdio>

int main(){
    printf("Hello world");
    return 0;
}

)";

    std::vector<const char*> compileArgs = {
        "clang++",
        "-std=c++20",
        "main.cpp",
        "-resource-dir",
        "/home/ykiko/C++/clice2/build/lib/clang/20",
    };

    Compiler compiler;
    compiler.buildPCH("main.cpp", code, compileArgs);

    auto invocation = createInvocation("main.cpp", code, compileArgs);
    compiler.applyPCH(*invocation, "main.cpp", code, "/home/ykiko/C++/clice2/build/cache/xxx.pch");
    auto instance = createInstance(std::move(invocation));
    auto action = std::make_unique<clang::SyntaxOnlyAction>();

    if(!action->BeginSourceFile(*instance, instance->getFrontendOpts().Inputs[0])) {
        llvm::errs() << "Failed to begin source file\n";
        std::terminate();
    }

    if(auto error = action->Execute()) {
        llvm::errs() << "Failed to execute action: " << error << "\n";
        std::terminate();
    }

    instance->getASTContext().getTranslationUnitDecl()->dump();
}

}  // namespace

