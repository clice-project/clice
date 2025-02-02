#include "Compiler/Command.h"
#include "Compiler/Compilation.h"

#include "clang/Lex/PreprocessorOptions.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"

namespace clice {

namespace impl {

std::unique_ptr<clang::CompilerInvocation> createInvocation(CompilationParams& params) {
    llvm::SmallString<1024> buffer;
    llvm::SmallVector<const char*, 16> args;

    if(auto error = mangleCommand(params.command, args, buffer)) {
        std::terminate();
    }

    clang::CreateInvocationOptions options = {};
    options.VFS = params.vfs;

    auto invocation = clang::createInvocation(args, options);
    if(!invocation) {
        std::terminate();
    }

    auto& frontOpts = invocation->getFrontendOpts();
    frontOpts.DisableFree = false;

    clang::LangOptions& langOpts = invocation->getLangOpts();
    langOpts.CommentOpts.ParseAllComments = true;
    langOpts.RetainCommentsFromSystemHeaders = true;

    return invocation;
}

std::unique_ptr<clang::CompilerInstance> createInstance(CompilationParams& params) {
    auto instance = std::make_unique<clang::CompilerInstance>();

    instance->setInvocation(createInvocation(params));

    /// TODO: use a thread safe filesystem and our customized `DiagnosticConsumer`.
    instance->createDiagnostics(
        *params.vfs,
        new clang::TextDiagnosticPrinter(llvm::outs(), new clang::DiagnosticOptions()),
        true);

    if(auto remapping = clang::createVFSFromCompilerInvocation(instance->getInvocation(),
                                                               instance->getDiagnostics(),
                                                               params.vfs)) {
        instance->createFileManager(std::move(remapping));
    }

    /// Add remapped files, if bounds is provided, cut off the content.
    std::size_t size = params.bound.has_value() ? params.bound.value() : params.content.size();

    assert(!instance->getPreprocessorOpts().RetainRemappedFileBuffers &&
           "RetainRemappedFileBuffers should be false");
    if(!params.content.empty()) {
        instance->getPreprocessorOpts().addRemappedFile(
            params.srcPath,
            llvm::MemoryBuffer::getMemBufferCopy(params.content.substr(0, size), params.srcPath)
                .release());
    }

    for(auto& [file, content]: params.remappedFiles) {
        instance->getPreprocessorOpts().addRemappedFile(
            file,
            llvm::MemoryBuffer::getMemBufferCopy(content, file).release());
    }

    if(!instance->createTarget()) {
        std::terminate();
    }

    auto [pch, bound] = params.pch;
    if(bound != 0) {
        auto& PPOpts = instance->getPreprocessorOpts();
        PPOpts.UsePredefines = false;
        PPOpts.ImplicitPCHInclude = std::move(pch);
        PPOpts.PrecompiledPreambleBytes = {bound, false};
        PPOpts.DisablePCHOrModuleValidation = clang::DisableValidationForModuleKind::PCH;
    }

    for(auto& [name, path]: params.pcms) {
        auto& HSOpts = instance->getHeaderSearchOpts();
        HSOpts.PrebuiltModuleFiles.try_emplace(name.str(), std::move(path));
    }

    return instance;
}

}  // namespace impl

namespace {

/// Execute given action with the on the given instance. `callback` is called after
/// `BeginSourceFile`. Beacuse `BeginSourceFile` may create new preprocessor.
llvm::Error ExecuteAction(clang::CompilerInstance& instance,
                          clang::FrontendAction& action,
                          auto&& callback) {
    if(!action.BeginSourceFile(instance, instance.getFrontendOpts().Inputs[0])) {
        return error("Failed to begin source file");
    }

    callback();

    if(auto error = action.Execute()) {
        return error;
    }

    return llvm::Error::success();
}

llvm::Expected<ASTInfo> ExecuteAction(std::unique_ptr<clang::CompilerInstance> instance,
                                      std::unique_ptr<clang::FrontendAction> action) {

    if(!action->BeginSourceFile(*instance, instance->getFrontendOpts().Inputs[0])) {
        return error("Failed to begin source file");
    }

    auto& pp = instance->getPreprocessor();
    // FIXME: clang-tidy, include-fixer, etc?

    // `BeginSourceFile` may create new preprocessor, so all operations related to preprocessor
    // should be done after `BeginSourceFile`.

    /// Collect directives.
    llvm::DenseMap<clang::FileID, Directive> directives;
    Directive::attach(pp, directives);

    /// Collect tokens.
    std::optional<clang::syntax::TokenCollector> tokCollector;

    /// It is not necessary to collect tokens if we are running code completion.
    /// And in fact will cause assertion failure.
    if(!instance->hasCodeCompletionConsumer()) {
        tokCollector.emplace(pp);
    }

    if(auto error = action->Execute()) {
        return clice::error("Failed to execute action, because {} ", error);
    }

    std::optional<clang::syntax::TokenBuffer> tokBuf;
    if(tokCollector) {
        tokBuf = std::move(*tokCollector).consume();
    }

    /// FIXME: getDependencies currently return ArrayRef<std::string>, which actually results in
    /// extra copy. It would be great to avoid this copy.

    std::optional<TemplateResolver> resolver;
    if(instance->hasSema()) {
        resolver.emplace(instance->getSema());
    }

    return ASTInfo(pp.getSourceManager().getMainFileID(),
                   std::move(action),
                   std::move(instance),
                   std::move(resolver),
                   std::move(tokBuf),
                   std::move(directives));
}

}  // namespace

llvm::Expected<ASTInfo> compile(CompilationParams& params) {
    auto instance = impl::createInstance(params);

    return ExecuteAction(std::move(instance), std::make_unique<clang::SyntaxOnlyAction>());
}

llvm::Expected<ASTInfo> compile(CompilationParams& params, clang::CodeCompleteConsumer* consumer) {
    auto instance = impl::createInstance(params);

    /// Set options to run code completion.
    instance->getFrontendOpts().CodeCompletionAt.FileName = params.srcPath.str();
    instance->getFrontendOpts().CodeCompletionAt.Line = params.line;
    instance->getFrontendOpts().CodeCompletionAt.Column = params.column;
    instance->setCodeCompletionConsumer(consumer);

    return ExecuteAction(std::move(instance), std::make_unique<clang::SyntaxOnlyAction>());
}

llvm::Expected<ASTInfo> compile(CompilationParams& params, PCHInfo& out) {
    assert(params.bound.has_value() && "Preamble bounds is required to build PCH");

    auto instance = impl::createInstance(params);

    llvm::StringRef outPath = params.outPath.str();

    /// Set options to generate PCH.
    instance->getFrontendOpts().OutputFile = outPath;
    instance->getFrontendOpts().ProgramAction = clang::frontend::GeneratePCH;
    instance->getPreprocessorOpts().GeneratePreamble = true;
    instance->getLangOpts().CompilingPCH = true;

    if(auto info =
           ExecuteAction(std::move(instance), std::make_unique<clang::GeneratePCHAction>())) {
        out.path = outPath;
        out.preamble = params.content.substr(0, *params.bound);
        out.command = params.command.str();
        out.deps = info->deps();

        return std::move(*info);
    } else {
        return info.takeError();
    }
}

llvm::Expected<ASTInfo> compile(CompilationParams& params, PCMInfo& out) {
    auto instance = impl::createInstance(params);

    /// Set options to generate PCM.
    instance->getFrontendOpts().OutputFile = params.outPath.str();
    instance->getFrontendOpts().ProgramAction = clang::frontend::GenerateReducedModuleInterface;

    ;
    if(auto info = ExecuteAction(std::move(instance),
                                 std::make_unique<clang::GenerateReducedModuleInterfaceAction>())) {
        assert(info->pp().isInNamedInterfaceUnit() &&
               "Only module interface unit could be built as PCM");
        out.isInterfaceUnit = true;
        out.name = info->pp().getNamedModuleName();
        for(auto& [name, path]: params.pcms) {
            out.mods.emplace_back(name);
        }
        out.path = params.outPath.str();
        out.srcPath = params.srcPath.str();
        return std::move(*info);
    } else {
        return info.takeError();
    }
}

}  // namespace clice
