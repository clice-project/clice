#include "../Test.h"
#include <Compiler/Compiler.h>
#include <Compiler//Selection.h>

namespace {

std::vector<const char*> compileArgs = {
    "clang++",
    "-std=c++20",
    "main.cpp",
    "-resource-dir",
    "/home/ykiko/C++/clice2/build/lib/clang/20",
};

using namespace clice;

TEST(clice, SelectionTree) {
    foreachFile("SelectionTree", [](std::string file, llvm::StringRef content) {
        auto compiler = Compiler("main.cpp", content, compileArgs);
        compiler.buildAST();
        auto& fileMgr = compiler.fileMgr();
        auto entry = fileMgr.getFileRef("main.cpp");
        if(!entry) {
            llvm::outs() << "Failed to get file id\n";
            std::terminate();
        }
        auto& srcMgr = compiler.srcMgr();
        auto id = srcMgr.translateFile(*entry);
        auto begin = srcMgr.translateLineCol(id, 7, 17);
        auto end = srcMgr.translateLineCol(id, 7, 17);
        SelectionTree tree(srcMgr.getFileOffset(begin),
                           srcMgr.getFileOffset(end),
                           compiler.context(),
                           compiler.tokBuf());
    });
}

}  // namespace

