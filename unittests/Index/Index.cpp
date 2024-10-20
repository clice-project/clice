#include <gtest/gtest.h>
#include <Index/Indexer.h>
#include <Index/Loader.h>
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
        Indexer slab(compiler.sema(), compiler.tokBuf());
        auto csif = slab.index();
        auto value = json::serialize(csif);
        std::error_code EC;
        llvm::raw_fd_ostream fileStream("output.json", EC);
        fileStream << value << "\n";

        llvm::outs() << "Index symbol count: " << csif.symbols.size() << "\n";
        // llvm::outs() << value << "\n";

        if(filepath.ends_with("ClassTemplate.cpp")) {
            Loader loader(csif, nullptr);
            Location location;
            location.range = {14, 1, 14, 2};
            auto& sym = loader.locate(location);
            llvm::outs() << sym.document << "\n";
        }
    });
}
