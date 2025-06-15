#include "CompilationUnitImpl.h"
#include "Compiler/Command.h"
#include "Compiler/Compilation.h"
#include "Compiler/Diagnostic.h"
#include "clang/Lex/PreprocessorOptions.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"

namespace clice {

namespace {

auto createInvocation(CompilationParams& params)
    -> std::expected<std::unique_ptr<clang::CompilerInvocation>, std::string> {
    llvm::SmallString<1024> buffer;
    llvm::SmallVector<const char*, 16> args;

    auto result = mangleCommand(params.command, args, buffer);
    if(!result) {
        return std::unexpected(std::move(result.error()));
    }

    std::vector<Diagnostic> diagnostics;
    auto engine = clang::CompilerInstance::createDiagnostics(*params.vfs,
                                                             new clang::DiagnosticOptions(),
                                                             Diagnostic::create(diagnostics));

    clang::CreateInvocationOptions options = {};
    options.VFS = params.vfs;
    options.Diags = engine;

    auto invocation = clang::createInvocation(args, options);
    if(!invocation) {
        /// FIXME: Use better way to render error.
        std::string error = "fail to create compiler invocation\n";
        for(auto& diagnostic: diagnostics) {
            error += std::format("{}\n", diagnostic.message);
        }
        return std::unexpected(std::move(error));
    }

    auto& frontOpts = invocation->getFrontendOpts();
    frontOpts.DisableFree = false;

    clang::LangOptions& langOpts = invocation->getLangOpts();
    langOpts.CommentOpts.ParseAllComments = true;
    langOpts.RetainCommentsFromSystemHeaders = true;

    return invocation;
}

class CliceASTConsumer : public clang::ASTConsumer {
public:
    CliceASTConsumer(std::vector<clang::Decl*>& top_level_decls,
                     const std::shared_ptr<std::atomic<bool>>& contiune_parse) :
        top_level_decls(top_level_decls), contiune_parse(contiune_parse) {}

    bool HandleTopLevelDecl(clang::DeclGroupRef group) override {
        for(auto decl: group) {
            top_level_decls.emplace_back(decl);
        }
        return *contiune_parse;
    }

private:
    std::vector<clang::Decl*>& top_level_decls;
    std::shared_ptr<std::atomic<bool>> contiune_parse;
};

class ProxyASTConsumer {};

class CliceFrontendAction : public clang::SyntaxOnlyAction {
public:
    CliceFrontendAction(std::unique_ptr<clang::ASTConsumer>& consumer) :
        consumer(std::move(consumer)) {}

    std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance& instance,
                                                          llvm::StringRef file) override {
        return std::move(consumer);
    }

private:
    std::unique_ptr<clang::ASTConsumer> consumer;
};

std::expected<std::unique_ptr<clang::CompilerInstance>, std::string>
    createInstance(CompilationParams& params) {
    auto instance = std::make_unique<clang::CompilerInstance>();

    auto invocation = createInvocation(params);
    if(!invocation) {
        return std::unexpected(invocation.error());
    }

    instance->setInvocation(std::move(*invocation));

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

    /// Add all remapped file.
    for(auto& [file, buffer]: params.buffers) {
        instance->getPreprocessorOpts().addRemappedFile(file, buffer.release());
    }
    params.buffers.clear();

    if(!instance->createTarget()) {
        std::abort();
    }

    auto [pch, bound] = params.pch;

    auto& PPOpts = instance->getPreprocessorOpts();
    PPOpts.ImplicitPCHInclude = std::move(pch);

    if(bound != 0) {
        PPOpts.PrecompiledPreambleBytes = {bound, false};
    }

    for(auto& [name, path]: params.pcms) {
        auto& HSOpts = instance->getHeaderSearchOpts();
        HSOpts.PrebuiltModuleFiles.try_emplace(name.str(), std::move(path));
    }

    return instance;
}

/// Execute given action with the on the given instance. `callback` is called after
/// `BeginSourceFile`. Beacuse `BeginSourceFile` may create new preprocessor.
std::expected<void, std::string> ExecuteAction(clang::CompilerInstance& instance,
                                               clang::FrontendAction& action,
                                               auto&& callback) {
    if(!action.BeginSourceFile(instance, instance.getFrontendOpts().Inputs[0])) {
        return std::unexpected("Failed to begin source file");
    }

    callback();

    if(auto error = action.Execute()) {
        return std::unexpected(std::format("Failed to execute action, because {} ", error));
    }

    return {};
}

std::expected<CompilationUnit, std::string>
    ExecuteAction(std::unique_ptr<clang::CompilerInstance> instance,
                  std::unique_ptr<clang::FrontendAction> action) {

    if(!action->BeginSourceFile(*instance, instance->getFrontendOpts().Inputs[0])) {
        return std::unexpected("Failed to begin source file");
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
        return std::unexpected(std::format("Failed to execute action, because {} ", error));
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

    auto impl = new CompilationUnit::Impl{
        .interested = pp.getSourceManager().getMainFileID(),
        .SM = instance->getSourceManager(),
        .action = std::move(action),
        .instance = std::move(instance),
        .m_resolver = std::move(resolver),
        .buffer = std::move(tokBuf),
        .m_directives = std::move(directives),
        .pathCache = llvm::DenseMap<clang::FileID, llvm::StringRef>(),
        .symbolHashCache = llvm::DenseMap<const void*, std::uint64_t>(),
    };

    return CompilationUnit(CompilationUnit::SyntaxOnly, impl);
}

}  // namespace

std::expected<CompilationUnit, std::string> preprocess(CompilationParams& params) {
    auto instance = createInstance(params);
    if(!instance) {
        return std::unexpected(std::move(instance.error()));
    }
    return ExecuteAction(std::move(*instance), std::make_unique<clang::PreprocessOnlyAction>());
}

std::expected<CompilationUnit, std::string> compile(CompilationParams& params) {
    auto instance = createInstance(params);
    if(!instance) {
        return std::unexpected(std::move(instance.error()));
    }
    return ExecuteAction(std::move(*instance), std::make_unique<clang::SyntaxOnlyAction>());
}

std::expected<CompilationUnit, std::string> complete(CompilationParams& params,
                                                     clang::CodeCompleteConsumer* consumer) {
    auto instance = createInstance(params);
    if(!instance) {
        return std::unexpected(std::move(instance.error()));
    }

    auto& [file, offset] = params.completion;

    /// The location of clang is 1-1 based.
    std::uint32_t line = 1;
    std::uint32_t column = 1;

    llvm::StringRef content;
    if(file == params.srcPath) {
        content = params.content;
    } else {
        auto it = params.buffers.find(file);
        assert(it != params.buffers.end() && "completion must occur in remapped file.");
        content = it->second->getBuffer();
    }

    for(auto c: content.substr(0, offset)) {
        if(c == '\n') {
            line += 1;
            column = 1;
            continue;
        }
        column += 1;
    }

    /// Set options to run code completion.
    instance.value()->getFrontendOpts().CodeCompletionAt.FileName = std::move(file);
    instance.value()->getFrontendOpts().CodeCompletionAt.Line = line;
    instance.value()->getFrontendOpts().CodeCompletionAt.Column = column;
    instance.value()->setCodeCompletionConsumer(consumer);

    return ExecuteAction(std::move(*instance), std::make_unique<clang::SyntaxOnlyAction>());
}

std::expected<CompilationUnit, std::string> compile(CompilationParams& params, PCHInfo& out) {
    assert(params.bound.has_value() && "Preamble bounds is required to build PCH");

    auto instance = createInstance(params);
    if(!instance) {
        return std::unexpected(std::move(instance.error()));
    }

    llvm::StringRef outPath = params.outPath.str();

    /// Set options to generate PCH.
    instance.value()->getFrontendOpts().OutputFile = outPath;
    instance.value()->getFrontendOpts().ProgramAction = clang::frontend::GeneratePCH;
    instance.value()->getPreprocessorOpts().GeneratePreamble = true;
    instance.value()->getLangOpts().CompilingPCH = true;

    if(auto info =
           ExecuteAction(std::move(*instance), std::make_unique<clang::GeneratePCHAction>())) {
        out.path = outPath;
        out.preamble = params.content.substr(0, *params.bound);
        out.command = params.command.str();
        out.deps = info->deps();
        return std::move(*info);
    } else {
        return std::unexpected(info.error());
    }
}

std::expected<CompilationUnit, std::string> compile(CompilationParams& params, PCMInfo& out) {
    auto instance = createInstance(params);
    if(!instance) {
        return std::unexpected(std::move(instance.error()));
    }

    /// Set options to generate PCM.
    instance.value()->getFrontendOpts().OutputFile = params.outPath.str();
    instance.value()->getFrontendOpts().ProgramAction =
        clang::frontend::GenerateReducedModuleInterface;

    if(auto info = ExecuteAction(std::move(*instance),
                                 std::make_unique<clang::GenerateReducedModuleInterfaceAction>())) {
        assert(info->is_module_interface_unit() &&
               "Only module interface unit could be built as PCM");
        out.isInterfaceUnit = true;
        out.name = info->module_name();
        for(auto& [name, path]: params.pcms) {
            out.mods.emplace_back(name);
        }
        out.path = params.outPath.str();
        out.srcPath = params.srcPath.str();
        return std::move(*info);
    } else {
        return std::unexpected(info.error());
    }
}

}  // namespace clice
