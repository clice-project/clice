#include "../Test.h"
#include <Compiler/Resolver.h>
#include <Compiler/Compiler.h>
#include <clang/Index/USRGeneration.h>
#include <Support/Reflection.h>

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
    using Base = clang::RecursiveASTVisitor<Visitor>;

    Visitor(llvm::StringRef content) : compiler("main.cpp", content, compileArgs) {
        compiler.buildAST();
    }

    bool shouldVisitTemplateInstantiations() const {
        return true;
    }

    bool VisitNamedDecl(clang::NamedDecl* decl) {
        llvm::outs() << "deck: " << decl->getName() << "\n";
        llvm::outs() << "linkage: " << refl::enum_name(decl->getLinkageInternal()) << "\n";
        decl->getFormalLinkage();
        llvm::SmallString<128> USR;
        clang::index::generateUSRForDecl(decl, USR);
        llvm::outs() << "USR: " << USR << "\n";
        return true;
    }

    void test() {
        auto decl = compiler.context().getTranslationUnitDecl();
        TraverseDecl(decl);
        // EXPECT_EQ(result.getCanonicalType(), expect.getCanonicalType());
        // compiler.context().getTranslationUnitDecl()->dump();
    }
};

TEST(clice, ASTVisitor) {
    foreachFile("ASTVisitor", [&](std::string file, llvm::StringRef content) {
        Visitor visitor(content);
        visitor.test();
    });
}

}  // namespace

