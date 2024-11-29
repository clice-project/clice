#include <Compiler/Compiler.h>
#include <clang/Lex/PreprocessorOptions.h>
#include <clang/Frontend/TextDiagnosticPrinter.h>

namespace clice {

static void adjustInvocation(clang::CompilerInvocation& invocation) {
    auto& frontOpts = invocation.getFrontendOpts();
    frontOpts.DisableFree = false;

    clang::LangOptions& langOpts = invocation.getLangOpts();
    langOpts.CommentOpts.ParseAllComments = true;
    langOpts.RetainCommentsFromSystemHeaders = true;

    // FIXME: add more.
}

Compiler::Compiler(llvm::StringRef filepath,
                   llvm::StringRef content,
                   llvm::ArrayRef<const char*> args,
                   clang::DiagnosticConsumer* consumer,
                   llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem> vfs) :
    filepath(filepath), content(content) {
    // FIXME: figure out should we use createInvocation?
    clang::CreateInvocationOptions options;
    auto invocation = clang::createInvocation(args, options);

    /// FIXME: use a thread safe for every thread.
    instance = std::make_unique<clang::CompilerInstance>(
        std::make_shared<clang::PCHContainerOperations>());
    adjustInvocation(*invocation);

    instance->setInvocation(std::move(invocation));

    // FIXME: customize DiagnosticConsumer
    if(consumer) {
        instance->createDiagnostics(*vfs, consumer, true);
    } else {
        instance->createDiagnostics(
            *vfs,
            new clang::TextDiagnosticPrinter(llvm::outs(), new clang::DiagnosticOptions()),
            true);
    }
}

bool Compiler::applyPCH(llvm::StringRef filepath, std::uint32_t bound, bool endAtStart) {
    // FIXME: check reuseable?
    auto& preproc = instance->getPreprocessorOpts();
    preproc.UsePredefines = false;
    preproc.ImplicitPCHInclude = filepath;
    preproc.PrecompiledPreambleBytes.first = bound;
    preproc.PrecompiledPreambleBytes.second = endAtStart;
    preproc.DisablePCHOrModuleValidation = clang::DisableValidationForModuleKind::PCH;
    return true;
}

bool Compiler::applyPCM(llvm::StringRef filepath, llvm::StringRef name) {
    // FIXME: check reuseable?
    instance->getHeaderSearchOpts().PrebuiltModuleFiles.try_emplace(name.str(), filepath);
    return true;
}

void Compiler::buildAST() {
    action = std::make_unique<clang::SyntaxOnlyAction>();
    instance->getFrontendOpts().DisableFree = false;
    ExecuteAction();
    m_Resolver = std::make_unique<TemplateResolver>(instance->getSema());
}

void Compiler::generatePCH(llvm::StringRef outpath, std::uint32_t bound, bool endAtStart) {
    content = content.substr(0, bound);
    instance->getFrontendOpts().OutputFile = outpath;
    instance->getFrontendOpts().ProgramAction = clang::frontend::GeneratePCH;
    instance->getPreprocessorOpts().PrecompiledPreambleBytes = {0, false};
    instance->getPreprocessorOpts().GeneratePreamble = true;
    instance->getLangOpts().CompilingPCH = true;
    action = std::make_unique<clang::GeneratePCHAction>();
    ExecuteAction();
}

void Compiler::generatePCM(llvm::StringRef outpath) {
    instance->getFrontendOpts().OutputFile = outpath;
    action = std::make_unique<clang::GenerateReducedModuleInterfaceAction>();
    ExecuteAction();
}

void Compiler::codeCompletion(llvm::StringRef filepath,
                              std::uint32_t line,
                              std::uint32_t column,
                              clang::CodeCompleteConsumer* consumer) {
    auto& location = instance->getFrontendOpts().CodeCompletionAt;
    location.FileName = filepath;
    location.Line = line;
    location.Column = column;

    auto buffer = llvm::MemoryBuffer::getMemBufferCopy(content);
    instance->getPreprocessorOpts().addRemappedFile(filepath, buffer.release());

    instance->setCodeCompletionConsumer(consumer);

    action = std::make_unique<clang::SyntaxOnlyAction>();

    if(!action->BeginSourceFile(*instance, instance->getFrontendOpts().Inputs[0])) {
        llvm::errs() << "Failed to begin source file\n";
        std::terminate();
    }

    /// instance->getASTContext().setExternalSource(nullptr);

    if(auto error = action->Execute()) {
        llvm::errs() << "Failed to execute action: " << error << "\n";
        std::terminate();
    }
}

void Compiler::ExecuteAction() {
    {
        auto buffer = llvm::MemoryBuffer::getMemBufferCopy(content);
        instance->getPreprocessorOpts().addRemappedFile(filepath, buffer.release());
    }

    if(auto VFSWithRemapping = createVFSFromCompilerInvocation(instance->getInvocation(),
                                                               instance->getDiagnostics(),
                                                               llvm::vfs::getRealFileSystem())) {
        instance->createFileManager(VFSWithRemapping);
    }

    if(!instance->createTarget()) {
        llvm::errs() << "Failed to create target\n";
        std::terminate();
    }

    llvm::outs() << instance->getLangOpts().Modules << "\n";
    llvm::outs() << instance->getLangOpts().CPlusPlusModules << "\n";

    if(!action->BeginSourceFile(*instance, instance->getFrontendOpts().Inputs[0])) {
        llvm::errs() << "Failed to begin source file\n";
        std::terminate();
    }

    /// llvm::outs() << instance->getPreprocessorOpts().ImplicitPCHInclude << "\n";

    /// instance->getASTContext().setExternalSource(nullptr);

    auto& preproc = instance->getPreprocessor();
    // FIXME: add PPCallbacks to collect information.

    // Beacuse CompilerInstance may create new Preprocessor in `BeginSourceFile`,
    // So we must need to create TokenCollector here.
    clang::syntax::TokenCollector collector{preproc};

    // FIXME: clang-tidy, include-fixer, etc?

    if(auto error = action->Execute()) {
        llvm::errs() << "Failed to execute action: " << error << "\n";
        std::terminate();
    }

    // Build TokenBuffer and index expanded tokens for improving performance.
    buffer = std::make_unique<clang::syntax::TokenBuffer>(std::move(collector).consume());
    buffer->indexExpandedTokens();
}

Compiler::~Compiler() {
    if(action) {
        action->EndSourceFile();
    }
}

}  // namespace clice
