#include <gtest/gtest.h>
#include <Support/JSON.h>
#include <Compiler/Compiler.h>
#include <Support/FileSystem.h>
#include <Compiler/Utility.h>
#include "../Test.h"
#include "clang/AST/DeclTemplate.h"
#include "Compiler/Semantic.h"
#include "Feature/SemanticTokens.h"

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

    bool VisitUnresolvedLookupExpr(clang::UnresolvedLookupExpr* expr) {
        if(srcMgr.isInMainFile(expr->getNameLoc())) {
            expr->dump();
        }
        return true;
    }

    bool VisitTemplateSpecializationTypeLoc(clang::TemplateSpecializationTypeLoc loc) {
        if(srcMgr.isInMainFile(loc.getLAngleLoc())) {
            loc.dump();
        }
        return true;
    }

    bool VisitDependentScopeDeclRefExpr(clang::DependentScopeDeclRefExpr* expr) {
        expr->dump();
        for(auto member: compiler.resolver().lookup(expr)) {
            member->dump();
        }
        clang::UserDefinedLiteral* literal;
        auto x = "x\n";
        return true;
    }

    bool VisitParmVarDecl(clang::ParmVarDecl* decl) {
        // decl->getTypeSourceInfo()->getTypeLoc().dump();
        return true;
    }

    bool VisitTypeLoc(clang::TypeLoc loc) {
        loc.dump();
        loc.getBeginLoc().dump(srcMgr);
        return true;
    }
};

TEST(clice, ASTVisitor) {
    foreachFile("ASTVisitor", [](std::string filepath, llvm::StringRef content) {
        if(filepath.ends_with("test.cpp")) {
            std::vector<const char*> compileArgs = {
                "clang++",
                "-std=c++23",
                filepath.c_str(),
                "-resource-dir",
                "/home/ykiko/C++/clice2/build/lib/clang/20",
            };

            auto bounds = clang::Lexer::ComputePreamble(content, {}, false);
            // auto start1 = std::chrono::steady_clock::now();
            //{
            //     Compiler compiler(filepath, content, compileArgs);
            //     compiler.generatePCH("/home/ykiko/C++/clice2/build/cache/xxx.pch",
            //                          bounds.Size,
            //                          bounds.PreambleEndsAtStartOfLine);
            // }
            // auto end1 = std::chrono::steady_clock::now();

            auto start2 = std::chrono::steady_clock::now();
            Compiler compiler(compileArgs);
            // compiler.applyPCH("/home/ykiko/C++/clice2/build/cache/xxx.pch",
            //                   bounds.Size,
            //                   bounds.PreambleEndsAtStartOfLine);
            compiler.buildAST();
            auto end2 = std::chrono::steady_clock::now();

            // ASTVisitor visitor(compiler, compiler.srcMgr(), compiler.sema());
            // visitor.TraverseAST(compiler.sema().getASTContext());

            auto start3 = std::chrono::steady_clock::now();
            auto r = feature::semanticTokens(compiler, filepath);
            auto end3 = std::chrono::steady_clock::now();

            // llvm::outs()
            //     << "Build PCH: "
            //     << std::chrono::duration_cast<std::chrono::milliseconds>(end1 - start1).count()
            //     << "ms\n";
            llvm::outs()
                << "Build AST: "
                << std::chrono::duration_cast<std::chrono::milliseconds>(end2 - start2).count()
                << "ms\n";
            llvm::outs()
                << "Semantic Tokens: "
                << std::chrono::duration_cast<std::chrono::milliseconds>(end3 - start3).count()
                << "ms\n";
        }
    });
}

}  // namespace

