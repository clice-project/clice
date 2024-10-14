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

Compiler::Compiler(llvm::StringRef filepath,
                   llvm::StringRef content,
                   llvm::ArrayRef<const char*> args,
                   llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem> vfs) : filepath(filepath), content(content) {
    // TODO: figure out should we use createInvocation?
    clang::CreateInvocationOptions options;
    auto invocation = clang::createInvocation(args, options);

    instance = std::make_unique<clang::CompilerInstance>();

    instance->setInvocation(std::move(invocation));

    instance->createDiagnostics(new clang::TextDiagnosticPrinter(llvm::outs(), new clang::DiagnosticOptions()), true);

    if(!instance->createTarget()) {
        llvm::errs() << "Failed to create target\n";
        std::terminate();
    }
}

Compiler::~Compiler() {
    if(action) {
        action->EndSourceFile();
    }
}

bool Compiler::applyPCH(llvm::StringRef filepath, std::uint32_t bound, bool endAtStart) {
    // TODO: check reuseable?
    auto& preproc = instance->getPreprocessorOpts();
    preproc.UsePredefines = false;
    preproc.ImplicitPCHInclude = filepath;
    preproc.PrecompiledPreambleBytes.first = bound;
    preproc.PrecompiledPreambleBytes.second = endAtStart;
    preproc.DisablePCHOrModuleValidation = clang::DisableValidationForModuleKind::PCH;
    return true;
}

bool Compiler::applyPCM(llvm::StringRef filepath, llvm::StringRef name) {
    // TODO: check reuseable?
    instance->getHeaderSearchOpts().PrebuiltModuleFiles.try_emplace(name.str(), filepath);
    return true;
}

void Compiler::generatePCH(llvm::StringRef outpath, std::uint32_t bound, bool endAtStart) {
    action = std::make_unique<clang::GeneratePCHAction>();

    instance->getFrontendOpts().OutputFile = outpath;

    auto buffer = llvm::MemoryBuffer::getMemBufferCopy(content.substr(0, bound));
    instance->getPreprocessorOpts().addRemappedFile(filepath, buffer.release());

    if(!action->BeginSourceFile(*instance, instance->getFrontendOpts().Inputs[0])) {
        llvm::errs() << "Failed to begin source file\n";
        std::terminate();
    }

    if(auto error = action->Execute()) {
        llvm::errs() << "Failed to execute action: " << error << "\n";
        std::terminate();
    }
}

void Compiler::generatePCM(llvm::StringRef outpath) {
    action = std::make_unique<clang::GenerateReducedModuleInterfaceAction>();

    instance->getFrontendOpts().OutputFile = outpath;

    auto buffer = llvm::MemoryBuffer::getMemBufferCopy(content);
    instance->getPreprocessorOpts().addRemappedFile(filepath, buffer.release());

    llvm::outs() << "file: " << filepath << "\n";

    if(!action->BeginSourceFile(*instance, instance->getFrontendOpts().Inputs[0])) {
        llvm::errs() << "Failed to begin source file\n";
        std::terminate();
    }

    if(auto error = action->Execute()) {
        llvm::errs() << "Failed to execute action: " << error << "\n";
        std::terminate();
    }
}

void Compiler::codeCompletion(llvm::StringRef filepath,
                              std::uint32_t line,
                              std::uint32_t column,
                              clang::CodeCompleteConsumer* consumer) {
    instance->setCodeCompletionConsumer(consumer);

    auto& codeComplete = instance->getFrontendOpts().CodeCompletionAt;
    codeComplete.FileName = filepath;
    codeComplete.Line = line;
    codeComplete.Column = column;

    buildAST();
}

void Compiler::buildAST() {
    action = std::make_unique<clang::SyntaxOnlyAction>();

    auto buffer = llvm::MemoryBuffer::getMemBufferCopy(content);
    instance->getPreprocessorOpts().addRemappedFile(filepath, buffer.release());

    if(!action->BeginSourceFile(*instance, instance->getFrontendOpts().Inputs[0])) {
        llvm::errs() << "Failed to begin source file\n";
        std::terminate();
    }

    if(auto error = action->Execute()) {
        llvm::errs() << "Failed to execute action: " << error << "\n";
        std::terminate();
    }
}

}  // namespace clice
