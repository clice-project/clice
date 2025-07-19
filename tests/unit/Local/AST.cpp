#include "AST/Utility.h"
#include "Test/CTest.h"
#include "Index/USR.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include <math.h>
#include "Test/LocationChain.h"
#include "Server/LSPConverter.h"

namespace clice::testing {

namespace {

struct Visitor : public clang::RecursiveASTVisitor<Visitor> {
public:
    bool shouldVisitTemplateInstantiations() const {
        return true;
    }

    bool VisitCallExpr(const clang::CallExpr* expr) {
        if(auto c = expr->getDirectCallee()) {
            c->dump();
            for(auto p: c->parameters()) {
                p->dump();
            }
        }

        return true;
    }

    bool VisitCXXConstructExpr(const clang::CXXConstructExpr* expr) {
        expr->dump();
        return true;
    }

    bool VisitInitListExpr(const clang::InitListExpr* expr) {
        clang::SubstTemplateTypeParmType* u;
        expr->dump();
        return true;
    }

    bool VisitVarDecl(const clang::VarDecl* decl) {
        // decl->getType()->dump();
        // auto loc = decl->getTypeSourceInfo()->getTypeLoc();
        // auto TTPT = loc.getAs<clang::SubstTemplateTypeParmTypeLoc>();
        // decl->getTypeSourceInfo()->getTypeLoc().getBeginLoc().dump(
        //     decl->getASTContext().getSourceManager());
        // auto& SM = unit.srcMgr();
        // decl->getLocation().dump(SM);
        // auto loc = decl->getLocation();
        // auto [fid, offset] = unit.getDecomposedLoc(decl->getLocation());
        // auto entry = SM.getFileEntryRefForID(fid);
        //
        // println("file: {}, offset: {}", SM.getFilename(loc), offset);
        return true;
    }

    bool VisitDeclRefExpr(const clang::DeclRefExpr* expr) {
        // auto& SM = unit.srcMgr();
        // expr->getExprLoc().dump(SM);
        // expr->dump();
        // llvm::outs() << "-------------------------------------\n";
        // expr->getDecl()->dump();

        return true;
    }

    CompilationUnit& unit;
};

TEST(Local, AST) {
    /// auto file = llvm::MemoryBuffer::getFile("/home/ykiko/C++/clice/temp/test.cpp");
    llvm::StringRef content = R"(
#line 777 "test.cpp"
int x = 1;
)";

    /// TODO: 主要就是测试一下 clang 的那个 CodeCompletion 里面的 pos 的 line 和 column
    /// 是怎么个计算规则， 考虑把接口转换成 offset，

    Tester tester("main.cpp", content);
    tester.addFile("./test.h", "");
    tester.compile("-std=c++23 -DCLICE=1 -include test.h");

    /// auto offset = tester.offset("0");
    /// LSPConverter converter;
    /// auto pos = converter.convert(tester.sources[0], offset);
    /// println("{}", dump(pos));
    /// println("offset1: {}, offset2: {}", offset, converter.convert(tester.sources[0], pos));

    auto& Ctx = tester.unit->context();
    Visitor visitor{.unit = *tester.unit};
    visitor.TraverseDecl(Ctx.getTranslationUnitDecl());

    tester.unit->tu()->dump();
}

void buildPreamble(llvm::StringRef content) {
    auto time1 = std::chrono::system_clock::now();

    // auto bounds = computePreambleBounds(content);
    // CompilationParams params;
    // params.content = content;
    //
    // params.command = "clang++ pch.cpp";
    // params.srcPath = "pch.cpp";
    //
    // params.bound = 67;
    // params.outPath = "./temp/preamble.pch";
    //
    //{
    //    PCHInfo info;
    //    auto result = compile(params, info);
    //    EXPECT_TRUE(bool(result));
    //}
    //
    // auto time2 = std::chrono::system_clock::now();
    //
    // params.pch = {"/home/ykiko/C++/clice/temp/preamble.pch", *params.bound};
    // params.bound.reset();
    //
    // params.command = "clang++ main.cpp";
    // params.srcPath = "main.cpp";
    //
    // auto unit = compile(params);
    // EXPECT_TRUE(bool(unit));
    //
    // Visitor visitor{.unit = *unit};
    // for(auto decl: unit->tu()->noload_decls()) {
    //    visitor.TraverseDecl(decl);
    //}

    // visitor.TraverseDecl(unit->tu());

    // auto time3 = std::chrono::system_clock::now();
    //
    // println("build Preamble consumes: {}, build AST consumes {}",
    //        duration_cast<std::chrono::milliseconds>(time2 - time1),
    //        duration_cast<std::chrono::milliseconds>(time3 - time2));
}

// void buildChain(llvm::StringRef content) {
//     auto time1 = std::chrono::system_clock::now();
//
//     auto bounds = computePreambleBounds(content);
//     CompilationParams params;
//     params.srcPath = "main.cpp";
//     params.command = "clang++ -x c++-header main.cpp";
//
//     for(auto i = 0; i < bounds.size(); i++) {
//         if(i != 0) {
//             params.pch = {params.outPath.str().str(), *params.bound};
//         }
//
//         params.content = content;
//         params.bound = bounds[i];
//         params.outPath = std::format("./temp/header{}.pch", i);
//
//         PCHInfo info;
//         auto AST = compile(params, info);
//         EXPECT_TRUE(bool(AST));
//     }
//
//     auto time2 = std::chrono::system_clock::now();
//
//     params.pch = {params.outPath.str().str(), *params.bound};
//     params.bound.reset();
//
//     auto AST = compile(params);
//     EXPECT_TRUE(bool(AST));
//
//     auto time3 = std::chrono::system_clock::now();
//
//     println("build PCH chain consumes: {}, build AST consumes {}",
//             duration_cast<std::chrono::milliseconds>(time2 - time1),
//             duration_cast<std::chrono::milliseconds>(time3 - time2));
// }

TEST(Local, ChainedPCH) {
    llvm::StringRef content = R"(
#include <bits/c++config.h>
#include <math.h>

void func() {
    int __N(x) = 1;
}
)";

    auto err = fs::create_directory("./temp");
    // buildPreamble(content);
    // buildChain(content);

    buildPreamble(R"(
#include <cstdio>
#include <cmath>

void foo(){
    int x = 2;
}

void bar();

int main(){
    foo();
}

)");
}

TEST(Local, Format) {
    println("{0:05}", 120);
    println("{:+06d}", 120);
}

}  // namespace

}  // namespace clice::testing
