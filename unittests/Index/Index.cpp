#include <gtest/gtest.h>
// #include <Index/Indexer.h>
// #include <Index/Loader.h>
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
        if(filepath.ends_with("ClassTemplate.cpp")) {
            // Compiler compiler("main.cpp", content, compileArgs);
            // compiler.buildAST();
            // index::Indexer slab(compiler.sema(), compiler.tokBuf());
            // auto csif = slab.index();
            // auto value = json::serialize(csif);
            // std::error_code EC;
            // llvm::raw_fd_ostream fileStream("output.json", EC);
            // fileStream << value << "\n";
            //
            // llvm::outs() << "Index symbol count: " << csif.symbols.size() << "\n";
            // // llvm::outs() << value << "\n";
            // index::Packer packer;
            // auto binary = packer.pack(csif);
            // llvm::outs() << "Binary size: " << binary.size() << "\n";
            //
            // index::Loader loader(binary.data());
            // index::in::Location location;
            // location = {14, 1, 14, 2, "main.cpp"};
            // auto sym = loader.locate(location);
            // llvm::outs() << "Symbol ID: " << sym.value << " USR: " << sym.USR << "\n";
        }
    });
}
