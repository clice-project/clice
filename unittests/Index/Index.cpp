#include <gtest/gtest.h>
#include <Support/FileSystem.h>
#include <Index/Serialize.h>
#include <Compiler/Compiler.h>

#include "../Test.h"

namespace {

using namespace clice;

TEST(Index, index) {
    foreachFile("Index", [](std::string filepath, llvm::StringRef content) {
        std::vector<const char*> compileArgs = {
            "clang++",
            "-std=c++20",
            filepath.c_str(),
            "-resource-dir",
            "/home/ykiko/C++/clice2/build/lib/clang/20",
        };
        if(filepath.ends_with("Vector.cpp")) {
            Compiler compiler(compileArgs);
            compiler.buildAST();
            auto index = index::index(compiler.sema());
            llvm::outs() << "files count:" << index.files.size() << "\n";
            auto json = index::toJson(index);
            // llvm::outs() << json << "\n";

            
            std::error_code error;
            llvm::raw_fd_ostream file("index.json", error);
            file << json;
            // auto file = index.files[0];
            // while(file.include.isValid()) {
            //     auto includeLoc = file.include;
            //     llvm::outs() << index.files[includeLoc.file].path << ":" << includeLoc.begin.line
            //     << ":" << includeLoc.begin.column << "\n"; file = index.files[file.include.file];
            // }
            // llvm::outs() << file.path << "\n";
        }
    });
}

}  // namespace

