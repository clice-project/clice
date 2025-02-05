#include "Test/CTest.h"
#include "Index/USR.h"
#include "clang/AST/RecursiveASTVisitor.h"

namespace clice::testing {

namespace {

struct ASTVisitor : public clang::RecursiveASTVisitor<ASTVisitor> {
    bool VisitDecl(clang::Decl* decl) {
        llvm::SmallString<128> buffer;
        if(!clice::index::generateUSRForDecl(decl, buffer)) {
            llvm::outs() << buffer << "\n";
        }
        return true;
    }
};

TEST(Index, USR) {
    const char* content = R"cpp(
int main() {
    return 0;
}    
)cpp";

    Tester tester("main.cpp", content);
    tester.run();

    auto& info = *tester.info;

    ASTVisitor visitor;
    visitor.TraverseDecl(info.tu());
}

}  // namespace

}  // namespace clice::testing
