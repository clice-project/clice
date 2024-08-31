#include "AST/ParsedAST.h"
#include "clang/Lex/PreprocessorOptions.h"

namespace clice {

std::unique_ptr<ParsedAST> ParsedAST::build(llvm::StringRef filename,
                                            llvm::StringRef content,
                                            std::vector<const char*>& args,
                                            Preamble* preamble) {
    auto vfs = llvm::vfs::getRealFileSystem();

    clang::CreateInvocationOptions options;
    // options.VFS = vfs;

    auto invocation = std::make_shared<clang::CompilerInvocation>();
    invocation = clang::createInvocation(args, options);

    auto buffer = llvm::MemoryBuffer::getMemBuffer(content, filename);
    if(preamble) {

        auto bounds = clang::ComputePreambleBounds(invocation->getLangOpts(), *buffer, false);

        // check if the preamble can be reused
        if(preamble->data.CanReuse(*invocation, *buffer, bounds, *vfs)) {
            llvm::outs() << "Resued preamble\n";
            // reuse preamble
            preamble->data.AddImplicitPreamble(*invocation, vfs, buffer.release());
        }
    } else {
        invocation->getPreprocessorOpts().addRemappedFile(invocation->getFrontendOpts().Inputs[0].getFile(),
                                                          buffer.release());
    }

    auto instance = std::make_unique<clang::CompilerInstance>();
    instance->setInvocation(std::move(invocation));

    instance->createDiagnostics(new Diagnostic(), true);

    // if(auto remappingVSF =
    //        createVFSFromCompilerInvocation(instance->getInvocation(), instance->getDiagnostics(), vfs)) {
    //     vfs = remappingVSF;
    // }
    // instance->createFileManager(vfs);

    if(!instance->createTarget()) {
        llvm::errs() << "Failed to create target\n";
        std::terminate();
    }

    auto action = std::make_unique<clang::SyntaxOnlyAction>();

    if(!action->BeginSourceFile(*instance, instance->getFrontendOpts().Inputs[0])) {
        llvm::errs() << "Failed to begin source file\n";
        std::terminate();
    }

    clang::syntax::TokenCollector collector(instance->getPreprocessor());

    if(auto error = action->Execute()) {
        llvm::errs() << "Failed to execute action: " << error << "\n";
        std::terminate();
    }

    auto result = new ParsedAST{
        .context = instance->getASTContext(),
        .preproc = instance->getPreprocessor(),
        .fileManager = instance->getFileManager(),
        .sourceManager = instance->getSourceManager(),
        .tokenBuffer = std::move(collector).consume(),
        .action = std::move(action),
        .instance = std::move(instance),

        .tuDecl = instance->getASTContext().getTranslationUnitDecl(),
    };

    return std::unique_ptr<ParsedAST>{result};
}

}  // namespace clice
