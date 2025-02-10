#include "Test/CTest.h"
#include "Index/USR.h"
#include "clang/AST/ODRHash.h"
#include "clang/AST/RecursiveASTVisitor.h"

namespace clice::testing {

namespace {

struct ASTVisitor : public clang::RecursiveASTVisitor<ASTVisitor> {
    bool VisitClassTemplatePartialSpecializationDecl(
        clang::ClassTemplatePartialSpecializationDecl* TD) {
        llvm::SmallString<128> buffer;
        if(!clice::index::generateUSRForDecl(TD, buffer)) {
            llvm::outs() << buffer << "\n";
        }

        return true;
    }

    // bool VisitDecl(const clang::Decl* decl){
    //     llvm::SmallString<128> buffer;
    //     if(!clice::index::generateUSRForDecl(decl, buffer)) {
    //         llvm::outs() << buffer << "\n";
    //     }
    //
    //    return true;
    //}
};

TEST(Index, USR) {
    const char* content = R"cpp(
template <typename T, typename U>
struct function;

template <typename T, typename U>
    requires(__is_same(U, int))
struct function<T, U> {
    void foo();
};

template <typename T, typename U>
    requires(__is_same(U, float))
struct function<T, U> {
    void foo();
};
)cpp";

    Tester tester("main.cpp", content);
    tester.run();

    auto& info = *tester.info;

    ASTVisitor visitor;
    visitor.TraverseDecl(info.tu());
}

}  // namespace

}  // namespace clice::testing
