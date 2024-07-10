#include <Clang/ParsedAST.h>
#include <Support/Logger.h>

namespace clice {

std::unique_ptr<ParsedAST> ParsedAST::build(std::string_view path, std::string_view content) {
    auto command = CompileDatabase::instance().lookup(path);
    std::shared_ptr<CompilerInvocation> invocation = clang::createInvocation(command);

    auto& inputs = invocation->getFrontendOpts().Inputs;
    inputs.push_back(clang::FrontendInputFile(path, clang::InputKind{clang::Language::CXX}));

    auto preamble = Preamble::build(path, content, *invocation);

    return build(path, invocation, preamble);
}

std::unique_ptr<ParsedAST> ParsedAST::build(std::string_view path,
                                            const std::shared_ptr<CompilerInvocation>& invocation,
                                            const Preamble& preamble) {
    auto AST = new ParsedAST();
    AST->path = path;

    // some settings for CompilerInstance
    auto& instance = AST->instance;
    instance.setInvocation(invocation);
    instance.createDiagnostics();

    if(!instance.createTarget()) {
        logger::error("Failed to create target");
    }

    if(auto manager = instance.createFileManager()) {
        instance.createSourceManager(*manager);
    } else {
        logger::error("Failed to create file manager");
    }

    instance.createPreprocessor(clang::TranslationUnitKind::TU_Complete);

    instance.createASTContext();

    // start FrontendAction
    const auto& input = instance.getFrontendOpts().Inputs[0];
    auto& action = AST->action;

    if(!action.BeginSourceFile(instance, input)) {
        logger::error("Failed to begin source file");
    }

    clang::syntax::TokenCollector collector = {instance.getPreprocessor()};

    if(llvm::Error error = action.Execute()) {
        logger::error("Failed to execute action: {0}", llvm::errorToErrorCode(std::move(error)).message());
    }

    AST->tokens.construct(std::move(collector).consume());

    return std::unique_ptr<ParsedAST>(AST);
}

}  // namespace clice
