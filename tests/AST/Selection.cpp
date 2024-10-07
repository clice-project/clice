#include "../Test.h"
#include <Compiler/Selection.h>

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
        auto AST = ParsedAST::build("main.cpp", content, compileArgs);
        auto id = AST->getFileID("main.cpp");
        auto& sm = AST->context.getSourceManager();
        auto begin = sm.translateLineCol(id, 1, 5);
        auto end = sm.translateLineCol(id, 1, 8);
        SelectionTree tree(AST->context, AST->tokenBuffer, begin, end);
    });
}

}  // namespace

