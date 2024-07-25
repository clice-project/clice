#include <Clang/Clang.h>

class ASTVistor : public clang::RecursiveASTVisitor<ASTVistor> {
private:
    clang::Preprocessor& preprocessor;
    clang::SourceManager& sourceManager;
    clang::syntax::TokenBuffer& buffer;

public:
    ASTVistor(clang::Preprocessor& preprocessor, clang::syntax::TokenBuffer& buffer) :
        preprocessor(preprocessor), sourceManager(preprocessor.getSourceManager()), buffer(buffer) {}

    bool VisitDecl(clang::Decl* decl) {
        if(clang::NamedDecl* named = llvm::dyn_cast<clang::NamedDecl>(decl)) {
            auto name = named->getName();
            if(name == "main") {
                llvm::outs() << "Found main function\n";
                auto location = named->getLocation();
                llvm::outs() << "line: " << sourceManager.getPresumedLineNumber(location) << "\n";
                llvm::outs() << "column: " << sourceManager.getPresumedColumnNumber(location) << "\n";
                llvm::outs() << "line: " << sourceManager.getSpellingLineNumber(location) << "\n";
                llvm::outs() << "column: " << sourceManager.getSpellingColumnNumber(location) << "\n";
                // auto token = buffer.spelledTokenAt(location);
            }
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
        argv[1],
    };

    invocation = clang::createInvocation(args, {});
    // clang::CompilerInvocation::CreateFromArgs(*invocation, args, instance->getDiagnostics());
    instance->setInvocation(std::move(invocation));

    if(!instance->createTarget()) {
        llvm::errs() << "Failed to create target\n";
        std::terminate();
    }

    clang::SyntaxOnlyAction action;

    if(!action.BeginSourceFile(*instance, instance->getFrontendOpts().Inputs[0])) {
        llvm::errs() << "Failed to begin source file\n";
        std::terminate();
    }

    clang::syntax::TokenCollector collector{instance->getPreprocessor()};

    if(auto error = action.Execute()) {
        llvm::errs() << "Failed to execute action: " << error << "\n";
        std::terminate();
    }

    clang::syntax::TokenBuffer buffer = std::move(collector).consume();

    auto tu = instance->getASTContext().getTranslationUnitDecl();
    ASTVistor visitor{instance->getPreprocessor(), buffer};
    visitor.TraverseDecl(tu);

    action.EndSourceFile();
}
