#include <gtest/gtest.h>
#include <Support/JSON.h>
#include <Compiler/Compiler.h>
#include <Support/FileSystem.h>
#include <Compiler//Utility.h>
#include "../Test.h"
#include "clang/AST/DeclTemplate.h"

namespace {

using namespace clice;

class ASTVisitor : public clang::RecursiveASTVisitor<ASTVisitor> {
public:
    clang::SourceManager& srcMgr;

    ASTVisitor(clang::SourceManager& srcMgr) : srcMgr(srcMgr) {}

    using Base = clang::RecursiveASTVisitor<ASTVisitor>;

    bool VisitDependentNameTypeLoc(clang::DependentNameTypeLoc loc) {
        loc.getNameLoc().dump(srcMgr);
        /// loc.dump();
        return true;
    }

    bool VisitDependentScopeDeclRefExpr(clang::DependentScopeDeclRefExpr* expr) {
        expr->dump();
        
        return true;
    }

    bool VisitCXXDependentScopeMemberExpr(clang::CXXDependentScopeMemberExpr* expr) {
        expr->dump();
        return true;
    }

    bool TraverseNestedNameSpecifierLoc(clang::NestedNameSpecifierLoc loc) {
        if(!loc) {
            return true;
        }

        auto NNS = loc.getNestedNameSpecifier();
        switch(NNS->getKind()) {
            case clang::NestedNameSpecifier::Identifier: {
                loc.getLocalSourceRange().dump(srcMgr);
                llvm::outs() << NNS << "\n";
                break;
            }
        }
        return Base::TraverseNestedNameSpecifierLoc(loc);
    }

    bool $VisitFunctionTemplateDecl(clang::FunctionTemplateDecl* decl) {
        for(auto spec: decl->specializations()) {
            spec->getLocation().dump(srcMgr);
            spec->getFunctionTypeLoc().getReturnLoc().getLocalSourceRange().dump(srcMgr);
            spec->getQualifierLoc().getSourceRange().dump(srcMgr);

            auto info = spec->getTemplateSpecializationInfo();
            info->PointOfInstantiation.dump(srcMgr);
            llvm::outs() << info->TemplateArguments << "\n";
            llvm::outs() << info->TemplateArgumentsAsWritten << "\n";
        }
        return true;
    }

    bool VisitVarTemplateSpecializationDecl(clang::VarTemplateSpecializationDecl* decl) {
        decl->dump();
        decl->getTypeSourceInfo()->getTypeLoc().getSourceRange().dump(srcMgr);
        return true;
    }

    bool $VisitNamedDecl(clang::NamedDecl* decl) {
        auto loc = decl->getLocation();
        /// is the token generated from a macro argument?
        llvm::outs() << "is in macro arg: " << srcMgr.isMacroArgExpansion(loc) << "\n";
        llvm::outs() << "is in macro body: " << srcMgr.isMacroBodyExpansion(loc) << "\n";
        auto range = srcMgr.getImmediateExpansionRange(loc);
        auto rang2 = srcMgr.getImmediateMacroCallerLoc(loc);
        decl->getLocation().dump(srcMgr);
        return true;
    };

    bool $VisitTemplateSpecializationTypeLoc(clang::TemplateSpecializationTypeLoc loc) {
        auto RD = loc.getType()->getAsCXXRecordDecl();
        llvm::outs() << loc.getTypePtr()->getTemplateName().getAsTemplateDecl()->getCanonicalDecl()
                     << "\n";
        llvm::outs() << loc.getType().getAsString() << "\n";
        instantiatedFrom(RD)->dump();
        llvm::outs() << "------------------------------------------------\n";
        return true;
    }

    bool $VisitMemberExpr(clang::MemberExpr* expr) {
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
            // compiler.tu()->dump();
        }
    });
}

}  // namespace

