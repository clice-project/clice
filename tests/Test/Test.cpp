#include <Test/Test.h>
#include <Compiler/Clang.h>

namespace clice::test {

namespace {

class TestEvaluator : public clang::RecursiveASTVisitor<TestEvaluator> {
    using Base = clang::RecursiveASTVisitor<TestEvaluator>;

public:
    explicit TestEvaluator(clang::ASTContext& context) : context(context) {}

    // bool TraverseDecl(clang::Decl* decl) {
    //     if(llvm::isa_and_nonnull<clang::TranslationUnitDecl>(decl)) {
    //         return Base::TraverseDecl(decl);
    //     }
    //
    //    if(auto ND = llvm::dyn_cast<clang::NamespaceDecl>(decl)) {
    //        if(ND->getName() == "Cases") {
    //            return Base::TraverseDecl(decl);
    //        }
    //    }
    //
    //    return true;
    //}

    bool VisitCXXConstructExpr(clang::CXXConstructExpr* expr) {
        auto type = expr->getType();
        auto iter = hooks.find(type.getAsString());

        if(iter == hooks.end()) {
            llvm::outs() << "hook not found: " << type.getAsString() << "\n";
            llvm::outs() << "size: " << hooks.size() << " address: " << &hooks << "\n";
            for(auto& [name, hook]: hooks) {
                llvm::outs() << name << "\n";
            }
            throw std::runtime_error("can not found hook function");
        }

        auto func = iter->second;
        llvm::SmallVector<clang::APValue, 4> args;
        for(auto arg: expr->arguments()) {
            clang::Expr::EvalResult result;
            if(!arg->EvaluateAsRValue(result, context)) {
                throw std::runtime_error("can not evaluate as rvalue");
            }
            args.push_back(result.Val);
        }
        func(args.data(), args.size());
        return true;
    }

private:
    clang::ASTContext& context;
};

}  // namespace

}  // namespace clice::test

namespace clice {
llvm::StringMap<bool (*)(clang::APValue*, std::size_t)> hooks;

bool exec(clang::ASTContext& context) {
    test::TestEvaluator evaluator(context);
    evaluator.TraverseAST(context);
    return true;
}

}  // namespace clice
