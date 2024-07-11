#include <Clang/Clang.h>

class ASTVistor : public clang::RecursiveASTVisitor<ASTVistor> {
public:
    bool VisitDecl(clang::Decl* decl) {
        if(clang::NamedDecl* named = llvm::dyn_cast<clang::NamedDecl>(decl)) {
            llvm::outs() << "Decl: " << named->getNameAsString() << "\n";
        }
        return true;
    }
};

int main(int argc, const char** argv) {
    assert(argc == 2 && "Usage: Preprocessor <source-file>");
    llvm::outs() << "running ASTVisitor...\n";

    auto instance = std::make_unique<clang::CompilerInstance>();

    clang::DiagnosticIDs* ids = new clang::DiagnosticIDs();
    clang::DiagnosticOptions* diag_opts = new clang::DiagnosticOptions();
    clang::DiagnosticConsumer* consumer = new clang::TextDiagnosticPrinter(llvm::errs(), diag_opts);
    clang::DiagnosticsEngine* engine = new clang::DiagnosticsEngine(ids, diag_opts, consumer);
    instance->setDiagnostics(engine);

    auto invocation = std::make_shared<clang::CompilerInvocation>();
    std::vector<const char*> args = {
        "/home/ykiko/Project/C++/clice/external/llvm/bin/clang++",
        "-Xclang",
        "-no-round-trip-args",
        "-std=c++20",
        "-Wno-everything",
        argv[1],
        "-c",
    };

    invocation = clang::createInvocation(args, {});
    // clang::CompilerInvocation::CreateFromArgs(*invocation, args, instance->getDiagnostics());
    instance->setInvocation(std::move(invocation));

    if(!instance->createTarget()) {
        llvm::errs() << "Failed to create target\n";
        std::terminate();
    }

    if(clang::FileManager* manager = instance->createFileManager()) {
        instance->createSourceManager(*manager);
    } else {
        llvm::errs() << "Failed to create file manager\n";
        std::terminate();
    }

    instance->createPreprocessor(clang::TranslationUnitKind::TU_Complete);

    // ASTContent is necessary for SemanticAnalysis
    instance->createASTContext();

    clang::SyntaxOnlyAction action;

    if(!action.BeginSourceFile(*instance, instance->getFrontendOpts().Inputs[0])) {
        llvm::errs() << "Failed to begin source file\n";
        std::terminate();
    }

    if(auto error = action.Execute()) {
        llvm::errs() << "Failed to execute action: " << error << "\n";
        std::terminate();
    }

    auto tu = instance->getASTContext().getTranslationUnitDecl();
    ASTVistor visitor;
    visitor.TraverseDecl(tu);

    action.EndSourceFile();
}
