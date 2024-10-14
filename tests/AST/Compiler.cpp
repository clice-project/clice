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

TEST(clice, PCH) {
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

    auto bounds = clang::Lexer::ComputePreamble(code, {}, false);
    {
        Compiler compiler("main.cpp", code, compileArgs);
        compiler.generatePCH("/home/ykiko/C++/clice2/build/cache/xxx.pch",
                             bounds.Size,
                             bounds.PreambleEndsAtStartOfLine);
    }

    Compiler compiler("main.cpp", code, compileArgs);
    compiler.applyPCH("/home/ykiko/C++/clice2/build/cache/xxx.pch", bounds.Size, bounds.PreambleEndsAtStartOfLine);
    compiler.buildAST();
    compiler.tu()->dump();
}

TEST(clice, PCM) {
    const char* mod = R"(
export module M;

export constexpr int f() {
    return 42;
}
)";

    const char* code = R"(
module;

export module Main;

import M;

module: private;

int main() {
    constexpr int x = f();
    return x;
}
)";

    std::vector<const char*> compileArgs = {
        "clang++",
        "-std=c++20",
        "main.cpp",
        "-resource-dir",
        "/home/ykiko/C++/clice2/build/lib/clang/20",
    };

    {
        Compiler compiler("main.cpp", mod, compileArgs);
        compiler.generatePCM("/home/ykiko/C++/clice2/build/cache/M.pcm");
    }

    static Compiler compiler("main.cpp", code, compileArgs);
    compiler.applyPCM("/home/ykiko/C++/clice2/build/cache/M.pcm", "M");
    compiler.buildAST();
    compiler.tu()->dump();

    class ModuleVisitor : public clang::RecursiveASTVisitor<ModuleVisitor> {
    public:
        bool VisitImportDecl(clang::ImportDecl* decl) {
            for(auto loc: decl->getIdentifierLocs()) {
                loc.dump(compiler.srcMgr());
            }
            auto mod = decl->getImportedModule();
            mod->DefinitionLoc.dump(compiler.srcMgr());
            // llvm::outs() << "Module: " << decl->getImportedModule()->getFullModuleName() << "\n";

            return true;
        }
    };

    ModuleVisitor visitor;
    visitor.TraverseAST(compiler.context());

    // instance->getASTContext().getTranslationUnitDecl()->dump();
}

}  // namespace

