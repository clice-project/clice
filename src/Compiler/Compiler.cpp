#include <Compiler/Compiler.h>
#include <clang/Lex/PreprocessorOptions.h>
#include <clang/Frontend/TextDiagnosticPrinter.h>

namespace clice {

static void setInvocation(clang::CompilerInvocation& invocation) {
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

    instance = std::make_unique<clang::CompilerInstance>();

    instance->setInvocation(std::move(invocation));

    // FIXME: customize DiagnosticConsumer
    if(consumer) {
        instance->createDiagnostics(consumer, true);
    } else {
        instance->createDiagnostics(
            new clang::TextDiagnosticPrinter(llvm::outs(), new clang::DiagnosticOptions()),
            true);
    }

    if(!instance->createTarget()) {
        llvm::errs() << "Failed to create target\n";
        std::terminate();
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
    ExecuteAction();
}

void Compiler::generatePCH(llvm::StringRef outpath, std::uint32_t bound, bool endAtStart) {
    content = content.substr(0, bound);
    instance->getFrontendOpts().OutputFile = outpath;
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

    if(content != "") {
        auto buffer = llvm::MemoryBuffer::getMemBufferCopy(content);
        instance->getPreprocessorOpts().addRemappedFile(filepath, buffer.release());
    }

    instance->setCodeCompletionConsumer(consumer);

    action = std::make_unique<clang::SyntaxOnlyAction>();

    if(!action->BeginSourceFile(*instance, instance->getFrontendOpts().Inputs[0])) {
        llvm::errs() << "Failed to begin source file\n";
        std::terminate();
    }

    if(auto error = action->Execute()) {
        llvm::errs() << "Failed to execute action: " << error << "\n";
        std::terminate();
    }
}

void Compiler::ExecuteAction() {
    if(content != "") {
        auto buffer = llvm::MemoryBuffer::getMemBufferCopy(content);
        instance->getPreprocessorOpts().addRemappedFile(filepath, buffer.release());
    }

    if(!action->BeginSourceFile(*instance, instance->getFrontendOpts().Inputs[0])) {
        llvm::errs() << "Failed to begin source file\n";
        std::terminate();
    }

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
