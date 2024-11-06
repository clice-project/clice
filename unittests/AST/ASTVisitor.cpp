#include <gtest/gtest.h>
#include <Support/JSON.h>
#include <Compiler/Compiler.h>
#include <Support/FileSystem.h>
#include <AST/Utility.h>
#include "../Test.h"

namespace {

using namespace clice;

class ASTVisitor : public clang::RecursiveASTVisitor<ASTVisitor> {
public:
    clang::SourceManager& srcMgr;

    ASTVisitor(clang::SourceManager& srcMgr) : srcMgr(srcMgr) {}

    bool VisitNamedDecl$(clang::NamedDecl* decl) {
        auto loc = decl->getLocation();
        /// is the token generated from a macro argument?
        llvm::outs() << "is in macro arg: " << srcMgr.isMacroArgExpansion(loc) << "\n";
        llvm::outs() << "is in macro body: " << srcMgr.isMacroBodyExpansion(loc) << "\n";
        auto range = srcMgr.getImmediateExpansionRange(loc);
        auto rang2 = srcMgr.getImmediateMacroCallerLoc(loc);
        decl->getLocation().dump(srcMgr);
        return true;
    };

    bool VisitTemplateSpecializationTypeLoc(clang::TemplateSpecializationTypeLoc loc) {
        auto RD = loc.getType()->getAsCXXRecordDecl();
        llvm::outs() << loc.getTypePtr()->getTemplateName().getAsTemplateDecl()->getCanonicalDecl()
                     << "\n";
        llvm::outs() << loc.getType().getAsString() << "\n";
        instantiatedFrom(RD)->dump();
        llvm::outs() << "------------------------------------------------\n";
        return true;
    }

    bool VisitMemberExpr(clang::MemberExpr* expr) {
        expr->getMemberDecl()->dump();
        // llvm::outs() << "is in macro arg: " << srcMgr.isMacroArgExpansion(loc) << "\n";
        // llvm::outs() << "is in macro body: " << srcMgr.isMacroBodyExpansion(loc) << "\n";
        // auto range = srcMgr.getImmediateExpansionRange(loc);
        // auto rang2 = srcMgr.getImmediateMacroCallerLoc(loc);
        // expr->getLocation().dump(srcMgr);
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
            ASTVisitor visitor(compiler.srcMgr());
            visitor.TraverseAST(compiler.context());
        }
    });
}

}  // namespace

