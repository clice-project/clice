#include "../Test.h"
#include <Compiler/Resolver.h>

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
    std::unique_ptr<ParsedAST> parsedAST;

    Visitor(llvm::StringRef content) : parsedAST(ParsedAST::build("main.cpp", content, compileArgs)) {}

    bool VisitVarDecl(clang::VarDecl* decl) {
        if(decl->getName() == "x") {
            decl->getType()->dump();
        }

        return true;
    }

    void test() {
        auto decl = parsedAST->context.getTranslationUnitDecl();
        TraverseDecl(decl);
    }
};

TEST(clice, ASTVisitor) {
    foreachFile("ASTVisitor", [&](std::string file, llvm::StringRef content) {
        Visitor visitor(content);
        visitor.test();
    });
}

}  // namespace

