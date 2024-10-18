#include <gtest/gtest.h>
#include <Index/SymbolSlab.h>
#include <Support/JSON.h>
#include <Compiler/Compiler.h>
#include <Support/FileSystem.h>
#include "../Test.h"

using namespace clice;
std::vector<const char*> compileArgs = {
    "clang++",
    "-std=c++20",
    "main.cpp",
    "-resource-dir",
    "/home/ykiko/C++/clice2/build/lib/clang/20",
};

TEST(clice, Index) {
    foreachFile("Index", [](llvm::StringRef filepath, llvm::StringRef content) {
        Compiler compiler("main.cpp", content, compileArgs);
        compiler.buildAST();
        SymbolSlab slab(compiler.sema(), compiler.tokBuf());
        auto csif = slab.index();
        auto value = json::serialize(csif);
        std::error_code EC;
        llvm::raw_fd_ostream fileStream("output.json", EC);
        fileStream << value << "\n";
        llvm::outs() << value << "\n";
        // compiler.context().getTranslationUnitDecl()->dump();
    });
}
