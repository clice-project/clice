#include "CompilationUnitImpl.h"
#include "Compiler/Command.h"
#include "Compiler/Compilation.h"
#include "Compiler/Diagnostic.h"
#include "clang/Lex/PreprocessorOptions.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"

namespace clice {

namespace {

/// create a `clang::CompilerInvocation` for compilation, it set and reset
/// all necessary arguments and flags for clice compilation.
auto create_invocation(CompilationParams& params,
                       llvm::IntrusiveRefCntPtr<clang::DiagnosticsEngine>& diagnostic_engine)
    -> std::unique_ptr<clang::CompilerInvocation> {

    /// Create clang invocation.
    clang::CreateInvocationOptions options = {
        .Diags = diagnostic_engine,
        .VFS = params.vfs,

        /// Avoid replacing -include with -include-pch, also
        /// see https://github.com/clangd/clangd/issues/856.
        .ProbePrecompiled = false,
    };

    auto invocation = clang::createInvocation(params.arguments, options);
    if(!invocation) {
        return nullptr;
    }

    auto& pp_opts = invocation->getPreprocessorOpts();
    assert(!pp_opts.RetainRemappedFileBuffers && "RetainRemappedFileBuffers should be false");

    for(auto& [file, buffer]: params.buffers) {
        pp_opts.addRemappedFile(file, buffer.release());
    }
    params.buffers.clear();

    auto [pch, bound] = params.pch;
    pp_opts.ImplicitPCHInclude = std::move(pch);
    if(bound != 0) {
        pp_opts.PrecompiledPreambleBytes = {bound, false};
    }

    auto& header_search_opts = invocation->getHeaderSearchOpts();
    for(auto& [name, path]: params.pcms) {
        header_search_opts.PrebuiltModuleFiles.try_emplace(name.str(), std::move(path));
    }

    auto& front_opts = invocation->getFrontendOpts();
    front_opts.DisableFree = false;

    clang::LangOptions& langOpts = invocation->getLangOpts();
    langOpts.CommentOpts.ParseAllComments = true;
    langOpts.RetainCommentsFromSystemHeaders = true;

    return invocation;
}

template <typename Action, typename Adjuster>
CompilationResult run_clang(CompilationParams& params, const Adjuster& adjuster) {
    auto diagnostics = params.diagnostics;
    auto diagnostic_engine =
        clang::CompilerInstance::createDiagnostics(*params.vfs,
                                                   new clang::DiagnosticOptions(),
                                                   Diagnostic::create(diagnostics));

    auto invocation = create_invocation(params, diagnostic_engine);
    if(!invocation) {
        return std::unexpected("Fail to create compilation invocation!");
    }

    auto instance = std::make_unique<clang::CompilerInstance>();
    instance->setInvocation(std::move(invocation));
    instance->setDiagnostics(diagnostic_engine.get());

    if(auto remapping = clang::createVFSFromCompilerInvocation(instance->getInvocation(),
                                                               instance->getDiagnostics(),
                                                               params.vfs)) {
        instance->createFileManager(std::move(remapping));
    }

    if(!instance->createTarget()) {
        return std::unexpected("Fail to create target!");
    }

    /// Adjust the compiler instance, for example, set preamble or modules.
    adjuster(*instance);

    std::unique_ptr<clang::FrontendAction> action = std::make_unique<Action>();

    if(!action->BeginSourceFile(*instance, instance->getFrontendOpts().Inputs[0])) {
        return std::unexpected("Fail to begin source file");
    }

    auto& pp = instance->getPreprocessor();
    // FIXME: clang-tidy, include-fixer, etc?

    // `BeginSourceFile` may create new preprocessor, so all operations related to preprocessor
    // should be done after `BeginSourceFile`.

    /// Collect directives.
    llvm::DenseMap<clang::FileID, Directive> directives;
    Directive::attach(pp, directives);

    /// Collect tokens.
    std::optional<clang::syntax::TokenCollector> tok_collector;

    /// It is not necessary to collect tokens if we are running code completion.
    /// And in fact will cause assertion failure.
    if(!instance->hasCodeCompletionConsumer()) {
        tok_collector.emplace(pp);
    }

    if(auto error = action->Execute()) {
        return std::unexpected(std::format("Failed to execute action, because {} ", error));
    }

    /// If the output file is not empty, it represents that we are
    /// generating a PCH or PCM. If error occurs, the AST must be
    /// invalid to some extent, serialization of such AST may result
    /// in crash frequently. So forbidden it here and return as error.
    if(!instance->getFrontendOpts().OutputFile.empty() &&
       instance->getDiagnostics().hasErrorOccurred()) {
        action->EndSourceFile();
        return std::unexpected("Fail to build PCH or PCM, error occurs in compilation.");
    }

    std::optional<clang::syntax::TokenBuffer> tok_buf;
    if(tok_collector) {
        tok_buf = std::move(*tok_collector).consume();
    }

    /// FIXME: getDependencies currently return ArrayRef<std::string>, which actually results in
    /// extra copy. It would be great to avoid this copy.

    std::optional<TemplateResolver> resolver;
    if(instance->hasSema()) {
        resolver.emplace(instance->getSema());
    }

    auto impl = new CompilationUnit::Impl{
        .interested = pp.getSourceManager().getMainFileID(),
        .src_mgr = instance->getSourceManager(),
        .action = std::move(action),
        .instance = std::move(instance),
        .m_resolver = std::move(resolver),
        .buffer = std::move(tok_buf),
        .m_directives = std::move(directives),
        .pathCache = llvm::DenseMap<clang::FileID, llvm::StringRef>(),
        .symbolHashCache = llvm::DenseMap<const void*, std::uint64_t>(),
        .diagnostics = diagnostics,
    };

    return CompilationUnit(CompilationUnit::SyntaxOnly, impl);
}

}  // namespace

CompilationResult preprocess(CompilationParams& params) {
    return run_clang<clang::PreprocessOnlyAction>(params, [](auto&) {});
}

CompilationResult compile(CompilationParams& params) {
    return run_clang<clang::SyntaxOnlyAction>(params, [](auto&) {});
}

CompilationResult compile(CompilationParams& params, PCHInfo& out) {
    assert(!params.outPath.empty() && "PCH file path cannot be empty");

    out.path = params.outPath.str();
    /// out.preamble = params.content.substr(0, *params.bound);
    /// out.command = params.arguments.str();
    /// FIXME: out.deps = info->deps();

    return run_clang<clang::GeneratePCHAction>(params, [&](clang::CompilerInstance& instance) {
        /// Set options to generate PCH.
        instance.getFrontendOpts().OutputFile = params.outPath.str();
        instance.getFrontendOpts().ProgramAction = clang::frontend::GeneratePCH;
        instance.getPreprocessorOpts().GeneratePreamble = true;
        instance.getLangOpts().CompilingPCH = true;
    });
}

CompilationResult compile(CompilationParams& params, PCMInfo& out) {
    for(auto& [name, path]: params.pcms) {
        out.mods.emplace_back(name);
    }
    out.path = params.outPath.str();

    return run_clang<clang::GenerateReducedModuleInterfaceAction>(
        params,
        [&](clang::CompilerInstance& instance) {
            /// Set options to generate PCH.
            instance.getFrontendOpts().OutputFile = params.outPath.str();
            instance.getFrontendOpts().ProgramAction =
                clang::frontend::GenerateReducedModuleInterface;
            out.srcPath = instance.getFrontendOpts().Inputs[0].getFile();
        });
}

CompilationResult complete(CompilationParams& params, clang::CodeCompleteConsumer* consumer) {

    auto& [file, offset] = params.completion;

    /// The location of clang is 1-1 based.
    std::uint32_t line = 1;
    std::uint32_t column = 1;

    /// FIXME:
    assert(params.buffers.size() == 1);
    llvm::StringRef content = params.buffers.begin()->second->getBuffer();

    for(auto c: content.substr(0, offset)) {
        if(c == '\n') {
            line += 1;
            column = 1;
            continue;
        }
        column += 1;
    }

    return run_clang<clang::SyntaxOnlyAction>(params, [&](clang::CompilerInstance& instance) {
        /// Set options to run code completion.
        instance.getFrontendOpts().CodeCompletionAt.FileName = std::move(file);
        instance.getFrontendOpts().CodeCompletionAt.Line = line;
        instance.getFrontendOpts().CodeCompletionAt.Column = column;
        instance.setCodeCompletionConsumer(consumer);
    });
}

}  // namespace clice
