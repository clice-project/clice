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
    createInvocation(StringRef filename, StringRef content, llvm::ArrayRef<const char*> args, Preamble* preamble) {
    clang::CreateInvocationOptions options;
    // FIXME: explore VFS
    auto vfs = llvm::vfs::getRealFileSystem();

    // TODO: figure out should we use createInvocation?
    auto invocation = clang::createInvocation(args, options);

    // setInvocation(*invocation);

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

void buildModule() {
    clang::CompilerInvocation invocation;

    invocation.getLangOpts().SkipODRCheckInGMF = true;

    auto& search = invocation.getHeaderSearchOpts();
    search.ValidateASTInputFilesContent = true;

    // TODO: insert all module name with their corresponding module files.
    auto& prebuilts = search.PrebuiltModuleFiles;

    // TODO: must set pcm output file.
    auto& frontend = invocation.getFrontendOpts();
    frontend.OutputFile = "...";

    auto instance = createInstance(std::make_shared<clang::CompilerInvocation>(invocation));

    clang::GenerateReducedModuleInterfaceAction action;
    instance->ExecuteAction(action);

    // TODO: check this.
    instance->getDiagnostics().hasErrorOccurred();
}

void Compiler::buildPCH(llvm::StringRef filename, llvm::StringRef content, llvm::ArrayRef<const char*> args) {
    // compute preamble bounds, i.e. the range of the preamble.
    // if MaxLines set to false(0), i.e. limit the number of lines.
    auto bounds = clang::Lexer::ComputePreamble(content, {}, false);

    auto invocation = createInvocation(filename, content.substr(0, bounds.Size), args);

    auto instance = createInstance(std::move(invocation));

    auto& frontend = instance->getFrontendOpts();
    frontend.OutputFile = "/home/ykiko/C++/clice2/build/cache/xxx.pch";

    auto& preproc = instance->getPreprocessorOpts();
    preproc.PrecompiledPreambleBytes.first = 0;
    preproc.PrecompiledPreambleBytes.second = false;
    preproc.GeneratePreamble = true;

    llvm::outs() << "bounds size: " << bounds.Size << "\n";

    // use to collect information in the process of building preamble, such as include files and macros
    // TODO: inherit from clang::PreambleCallbacks and collect the information
    clang::PreambleCallbacks callbacks = {};

    auto action = clang::GeneratePCHAction();

    if(!action.BeginSourceFile(*instance, instance->getFrontendOpts().Inputs[0])) {
        llvm::errs() << "Failed to begin source file\n";
        std::terminate();
    }

    if(auto error = action.Execute()) {
        llvm::errs() << "Failed to execute action: " << error << "\n";
        std::terminate();
    }

    // instance->getASTContext().getTranslationUnitDecl()->dump();

    action.EndSourceFile();
}

void Compiler::applyPCH(clang::CompilerInvocation& invocation,
                        llvm::StringRef filename,
                        llvm::StringRef content,
                        llvm::StringRef filepath) {

    auto bounds = clang::Lexer::ComputePreamble(content, invocation.getLangOpts(), false);

    auto& PreprocessorOpts = invocation.getPreprocessorOpts();

    PreprocessorOpts.PrecompiledPreambleBytes.first = bounds.Size;
    PreprocessorOpts.PrecompiledPreambleBytes.second = bounds.PreambleEndsAtStartOfLine;
    PreprocessorOpts.DisablePCHOrModuleValidation = clang::DisableValidationForModuleKind::PCH;

    // key point
    PreprocessorOpts.ImplicitPCHInclude = filepath;

    PreprocessorOpts.UsePredefines = false;
}

void Compiler::buildPCM(llvm::StringRef filename, llvm::StringRef content, llvm::ArrayRef<const char*> args) {
    auto invocation = createInvocation(filename, content, args);

    auto instance = createInstance(std::move(invocation));

    auto& frontend = instance->getFrontendOpts();
    frontend.OutputFile = "/home/ykiko/C++/clice2/build/cache/xxx.pcm";

    auto action = clang::GenerateReducedModuleInterfaceAction();

    if(!action.BeginSourceFile(*instance, instance->getFrontendOpts().Inputs[0])) {
        llvm::errs() << "Failed to begin source file\n";
        std::terminate();
    }

    if(auto error = action.Execute()) {
        llvm::errs() << "Failed to execute action: " << error << "\n";
        std::terminate();
    }

    action.EndSourceFile();
}

}  // namespace clice
