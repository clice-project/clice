#include <gtest/gtest.h>
#include <Support/JSON.h>
#include <Compiler/Compiler.h>
#include <Support/FileSystem.h>
#include <Compiler/Utility.h>
#include "../Test.h"
#include "clang/AST/DeclTemplate.h"
#include "Compiler/Semantic.h"

namespace {

using namespace clice;

class ASTVisitor : public clang::RecursiveASTVisitor<ASTVisitor> {
public:
    Compiler& compiler;
    clang::SourceManager& srcMgr;
    clang::Sema& sema;

    ASTVisitor(Compiler& compiler, clang::SourceManager& srcMgr, clang::Sema& sema) :
        compiler(compiler), srcMgr(srcMgr), sema(sema) {}

    using Base = clang::RecursiveASTVisitor<ASTVisitor>;

    void dump(clang::SourceLocation loc) {
        if(loc.isMacroID()) {
            llvm::outs() << "expansion: ";
            srcMgr.getExpansionLoc(loc).print(llvm::outs(), srcMgr);
            llvm::outs() << "\n";

            llvm::outs() << "spelling: ";
            srcMgr.getSpellingLoc(loc).print(llvm::outs(), srcMgr);
            llvm::outs() << "\n";

            llvm::outs() << "file: ";
            srcMgr.getFileLoc(loc).print(llvm::outs(), srcMgr);
            llvm::outs() << "\n";

            auto [begin, end] = srcMgr.getImmediateExpansionRange(loc).getAsRange();
            if(begin.isValid() && end.isValid()) {
                llvm::outs() << "expansion Range: \n";
                dump(begin);
                dump(end);
                llvm::outs() << "\n";
            }

            llvm::outs() << "\n";
        }
    }

    // bool VisitTemplateSpecializationTypeLoc(clang::TemplateSpecializationTypeLoc loc) {
    //     loc.dump();
    //     return true;
    // }

    bool VisitDependentScopeDeclRefExpr(clang::DependentScopeDeclRefExpr* expr) {
        expr->dump();
        for(auto member: compiler.resolver().lookup(expr)) {
            member->dump();
        }
        return true;
    }
};

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

            SemanticVisitor visitor(compiler);
            visitor.TraverseAST(compiler.context());
            // ASTVisitor visitor(compiler, compiler.srcMgr(), compiler.sema());
            // visitor.TraverseAST(compiler.context());
            //  compiler.tu()->dump();
        }
    });
}

}  // namespace

