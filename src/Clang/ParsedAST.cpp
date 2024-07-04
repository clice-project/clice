#include <Clang/ParsedAST.h>

namespace clice {

std::unique_ptr<ParsedAST> ParsedAST::build(std::string_view path,
                                            const std::shared_ptr<CompilerInvocation>& invocation,
                                            const std::shared_ptr<Preamble>& preamble) {
    auto AST = new ParsedAST();
    AST->path = path;

    // some settings for CompilerInstance
    auto& instance = AST->instance;
    instance.setInvocation(invocation);
    instance.createDiagnostics();

    if(!instance.createTarget()) {
        llvm::errs() << "Failed to create target\n";
        std::terminate();
    }

    if(auto manager = instance.createFileManager()) {
        instance.createSourceManager(*manager);
    } else {
        llvm::errs() << "Failed to create file manager\n";
        std::terminate();
    }

    instance.createPreprocessor(clang::TranslationUnitKind::TU_Complete);

    instance.createASTContext();

    // start FrontendAction
    const auto& input = instance.getFrontendOpts().Inputs[0];
    auto& action = AST->action;

    if(!action.BeginSourceFile(instance, input)) {
        llvm::errs() << "Failed to begin source file\n";
        std::terminate();
    }

    clang::syntax::TokenCollector collector = {instance.getPreprocessor()};

    if(llvm::Error error = action.Execute()) {
        llvm::errs() << "Failed to execute action: " << error << "\n";
        std::terminate();
    }

    AST->tokens.construct(std::move(collector).consume());

    return std::unique_ptr<ParsedAST>(AST);
}

}  // namespace clice
