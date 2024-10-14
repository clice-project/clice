#include "../Test.h"
#include <Compiler/Resolver.h>
#include <Compiler/Compiler.h>

namespace {

using namespace clice;

std::vector<const char*> compileArgs = {
    "clang++",
    "-std=c++20",
    "main.cpp",
    "-resource-dir",
    "/home/ykiko/C++/clice2/build/lib/clang/20",
};

struct Visitor : public clang::RecursiveASTVisitor<Visitor> {
    clang::QualType result;
    clang::QualType expect;
    clice::Compiler compiler;

    Visitor(llvm::StringRef content) : compiler("main.cpp", content, compileArgs) {
        compiler.buildAST();
    }

    void test() {
        auto decl = compiler.context().getTranslationUnitDecl();
        TraverseDecl(decl);
        EXPECT_EQ(result.getCanonicalType(), expect.getCanonicalType());
    }
};

TEST(clice, ASTVisitor) {
    foreachFile("ASTVisitor", [&](std::string file, llvm::StringRef content) {
        Visitor visitor(content);
        visitor.test();
    });
}

}  // namespace

