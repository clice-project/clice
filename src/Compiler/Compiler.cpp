#include <Compiler/Compiler.h>
#include <Compiler/Preamble.h>

#include <clang/Lex/PreprocessorOptions.h>

namespace clice {

static void setInvocation(clang::CompilerInvocation& invocation) {
    clang::LangOptions& langOpts = invocation.getLangOpts();
    langOpts.CommentOpts.ParseAllComments = true;
    langOpts.RetainCommentsFromSystemHeaders = true;
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
    auto& codeComplete = instance->getFrontendOpts().CodeCompletionAt;
    codeComplete.FileName = filepath;
    codeComplete.Line = line;
    codeComplete.Column = column;
    instance->setCodeCompletionConsumer(consumer);
    buildAST();
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

    if(auto error = action->Execute()) {
        llvm::errs() << "Failed to execute action: " << error << "\n";
        std::terminate();
    }
}

Compiler::~Compiler() {
    if(action) {
        action->EndSourceFile();
    }
}

}  // namespace clice
