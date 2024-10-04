#include <Compiler/ParsedAST.h>
#include <Compiler/Compiler.h>

namespace clice {

std::unique_ptr<ParsedAST> ParsedAST::build(llvm::StringRef filename,
                                            llvm::StringRef content,
                                            std::vector<const char*>& args,
                                            Preamble* preamble,
                                            clang::CodeCompleteConsumer* consumer) {
    auto vfs = llvm::vfs::getRealFileSystem();

    clang::CreateInvocationOptions options;
    // options.VFS = vfs;

    auto invocation = createInvocation(filename, content, args, preamble);
    auto instance = createInstance(std::move(invocation));
    auto action = std::make_unique<clang::SyntaxOnlyAction>();

    if(!action->BeginSourceFile(*instance, instance->getFrontendOpts().Inputs[0])) {
        llvm::errs() << "Failed to begin source file\n";
        std::terminate();
    }

    auto& preproc = instance->getPreprocessor();
    clang::syntax::TokenCollector collector(preproc);

    auto directive = std::make_unique<Directives>(instance->getPreprocessor(), instance->getSourceManager());
    preproc.addCommentHandler(directive->handler());
    preproc.addPPCallbacks(directive->callback());

    if(consumer) {
        instance->setCodeCompletionConsumer(consumer);
    }

    if(auto error = action->Execute()) {
        llvm::errs() << "Failed to execute action: " << error << "\n";
        std::terminate();
    }

    auto result = new ParsedAST{
        .sema = instance->getSema(),
        .context = instance->getASTContext(),
        .preproc = instance->getPreprocessor(),
        .fileManager = instance->getFileManager(),
        .sourceManager = instance->getSourceManager(),
        .tokenBuffer = std::move(collector).consume(),
        .directive = std::move(directive),
        .action = std::move(action),
        .instance = std::move(instance),
    };

    return std::unique_ptr<ParsedAST>{result};
}

}  // namespace clice
