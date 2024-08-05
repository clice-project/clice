#include <Clang/Clang.h>
#include <clang/Sema/Lookup.h>
#include <clang/Sema/Template.h>
#include <TreeTransform.h>
#include <clang/Sema/SemaConsumer.h>

namespace clang {
class MyTreeTransform : public TreeTransform<MyTreeTransform> {
    ASTContext& Context;

public:
    MyTreeTransform(Sema& SemaRef, clang::ASTContext& Context) :
        TreeTransform<MyTreeTransform>(SemaRef), Context(Context) {}

    ExprResult TransformBinaryOperator(BinaryOperator* E) {
        llvm::outs() << "Transforming BinaryOperator\n";
        E->dump();
        auto LHS = TransformExpr(E->getLHS());
        auto RHS = TransformExpr(E->getRHS());
        return SemaRef.BuildBinOp(SemaRef.getCurScope(),
                                  E->getOperatorLoc(),
                                  E->getOpcode(),
                                  RHS.get(),
                                  LHS.get());
    }

    ExprResult RebuildBinaryOperator(SourceLocation OpLoc, BinaryOperatorKind Opc, Expr* LHS, Expr* RHS) {
        llvm::outs() << "-----------------------------" << "\n";
        return SemaRef.BuildBinOp(SemaRef.getCurScope(), OpLoc, Opc, RHS, LHS);
    }
};

class ASTVisitor : public RecursiveASTVisitor<ASTVisitor> {
    clang::ASTContext& Context;
    clang::Sema& SemaRef;

public:

public:
    ASTVisitor(clang::ASTContext& Context, clang::Sema& SemaRef) : Context(Context), SemaRef(SemaRef) {}

    bool VisitVarDecl(VarDecl* decl) {
        llvm::outs() << "Visiting BinaryOperator\n";
        clang::MyTreeTransform transformer{SemaRef, Context};
        decl->dump();
        auto result = transformer.TransformExpr(decl->getInit());
        decl->setInit(result.get());
        decl->dump();
        return true;
    }
};

class MySemaConsumer : public clang::SemaConsumer {
    clang::CompilerInstance& CI;

public:
    MySemaConsumer(clang::CompilerInstance& CI) : CI(CI) {}

    void HandleTranslationUnit(clang::ASTContext& Context) override {
        clang::Sema& SemaRef = CI.getSema();
        clang::MyTreeTransform Transformer(SemaRef, Context);
        llvm::outs() << "------------------------------\n";
        auto TU = Context.getTranslationUnitDecl();
        clang::ASTVisitor visitor{Context, SemaRef};
        visitor.TraverseDecl(TU);
    }
};

class MyFrontendAction : public clang::ASTFrontendAction {
public:
    std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance& CI,
                                                          llvm::StringRef) override {
        return std::make_unique<MySemaConsumer>(CI);
    }
};

}  // namespace clang

int main(int argc, const char** argv) {
    assert(argc == 2 && "Usage: Preprocessor <source-file>");
    llvm::outs() << "running ASTVisitor...\n";

    auto instance = std::make_unique<clang::CompilerInstance>();

    clang::DiagnosticIDs* ids = new clang::DiagnosticIDs();
    clang::DiagnosticOptions* diag_opts = new clang::DiagnosticOptions();
    clang::DiagnosticConsumer* consumer = new clang::IgnoringDiagConsumer();
    clang::DiagnosticsEngine* engine = new clang::DiagnosticsEngine(ids, diag_opts, consumer);
    instance->setDiagnostics(engine);

    auto invocation = std::make_shared<clang::CompilerInvocation>();
    std::vector<const char*> args = {
        "/usr/local/bin/clang++",
        "-Xclang",
        "-no-round-trip-args",
        "-std=c++20",
        argv[1],
    };

    invocation = clang::createInvocation(args, {});
    // clang::CompilerInvocation::CreateFromArgs(*invocation, args, instance->getDiagnostics());
    instance->setInvocation(std::move(invocation));

    if(!instance->createTarget()) {
        llvm::errs() << "Failed to create target\n";
        std::terminate();
    }

    // instance->setASTConsumer(std::make_unique<clang::MyASTConsumer>(*instance));

    clang::MyFrontendAction action;

    if(!action.BeginSourceFile(*instance, instance->getFrontendOpts().Inputs[0])) {
        llvm::errs() << "Failed to begin source file\n";
        std::terminate();
    }

    if(auto error = action.Execute()) {
        llvm::errs() << "Failed to execute action: " << error << "\n";
        std::terminate();
    }

    auto tu = instance->getASTContext().getTranslationUnitDecl();

    // tu->dump();
    //   ASTVistor visitor{instance->getPreprocessor(), buffer, instance->getASTContext(),
    //   instance->getSema()}; visitor.TraverseDecl(tu);

    action.EndSourceFile();
};
