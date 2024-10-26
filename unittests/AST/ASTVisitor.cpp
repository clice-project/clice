#include <gtest/gtest.h>
#include <Support/JSON.h>
#include <Compiler/Compiler.h>
#include <Support/FileSystem.h>
#include "../Test.h"

namespace {

using namespace clice;

TEST(clice, ASTVisitor) {
    foreachFile("ASTVisitor", [](std::string filepath, llvm::StringRef content) {
        if(filepath.ends_with("test.cpp")) {
            std::vector<const char*> compileArgs = {
                "clang++",
                "-std=c++20",
                filepath.c_str(),
                "-resource-dir",
                "/home/ykiko/C++/clice2/build/lib/clang/20",
            };
            Compiler compiler(compileArgs);
            compiler.buildAST();
            auto macros = compiler.pp().macros();
            for(auto& macro: macros) {
                auto& s = macro.first;
                auto& state = macro.second;
                state.getLatest();
                //  llvm::outs() << macro.first << " " << macro.second << "\n";
            }
            // compiler.tu()->dump();
        }
    });
}

}  // namespace

