#include <Compiler/Compiler.h>
#include <Compiler/Preamble.h>

#include <clang/Lex/PreprocessorOptions.h>

namespace clice {

static void setInvocation(clang::CompilerInvocation& invocation) {
    clang::LangOptions& langOpts = invocation.getLangOpts();
    langOpts.CommentOpts.ParseAllComments = true;
    langOpts.RetainCommentsFromSystemHeaders = true;
}

std::unique_ptr<clang::CompilerInvocation>
    createInvocation(StringRef filename, StringRef content, std::vector<const char*>& args, Preamble* preamble) {
    clang::CreateInvocationOptions options;
    // FIXME: explore VFS
    auto vfs = llvm::vfs::getRealFileSystem();

    auto invocation = clang::createInvocation(args, options);

    setInvocation(*invocation);

    auto buffer = llvm::MemoryBuffer::getMemBufferCopy(content, filename);
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

    return invocation;
}

std::unique_ptr<clang::CompilerInstance> createInstance(std::shared_ptr<clang::CompilerInvocation> invocation) {
    auto instance = std::make_unique<clang::CompilerInstance>();

    instance->setInvocation(std::move(invocation));
    // FIXME: resolve diagnostics
    instance->createDiagnostics(new clang::TextDiagnosticPrinter(llvm::outs(), new clang::DiagnosticOptions()), true);

    if(!instance->createTarget()) {
        llvm::errs() << "Failed to create target\n";
        std::terminate();
    }

    return instance;
}

}  // namespace clice
