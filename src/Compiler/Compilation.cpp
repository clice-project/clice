#include "CompilationUnitImpl.h"
#include "Compiler/Command.h"
#include "Compiler/Compilation.h"
#include "Compiler/Diagnostic.h"
#include "clang/Lex/PreprocessorOptions.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"

namespace clice {

#define TRY_OR_RETURN(expr)                                                                        \
    do {                                                                                           \
        auto& macro_result = (expr);                                                               \
        if(!macro_result.has_value()) {                                                            \
            return std::unexpected(std::move(macro_result.error()));                               \
        }                                                                                          \
    } while(0)

#define ASSIGN_OR_RETURN(var, expr)                                                                \
    do {                                                                                           \
        auto&& macro_result = (expr);                                                              \
        if(!macro_result.has_value()) {                                                            \
            return std::unexpected(std::move(macro_result.error()));                               \
        }                                                                                          \
        var = std::move(*macro_result);                                                            \
    } while(0)

namespace {

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

auto createInvocation(CompilationParams& params,
                      std::shared_ptr<std::vector<Diagnostic>>& diagnostics,
                      llvm::IntrusiveRefCntPtr<clang::DiagnosticsEngine>& diagnostic_engine)
    -> std::expected<std::unique_ptr<clang::CompilerInvocation>, std::string> {
    llvm::SmallString<1024> buffer;
    llvm::SmallVector<const char*, 16> args;

    auto result = mangleCommand(params.command, args, buffer);
    TRY_OR_RETURN(result);

    clang::CreateInvocationOptions options = {};
    options.VFS = params.vfs;
    options.Diags = diagnostic_engine;

    auto invocation = clang::createInvocation(args, options);
    if(!invocation) {
        /// FIXME: Use better way to render error.
        std::string error = "fail to create compiler invocation\n";
        for(auto& diagnostic: *diagnostics) {
            error += std::format("{}\n", diagnostic.message);
        }
        return std::unexpected(std::move(error));
    }

    auto& frontOpts = invocation->getFrontendOpts();
    frontOpts.DisableFree = false;

    clang::LangOptions& langOpts = invocation->getLangOpts();
    langOpts.CommentOpts.ParseAllComments = true;
    langOpts.RetainCommentsFromSystemHeaders = true;

    auto& PPOpts = invocation->getPreprocessorOpts();

    if(!params.content.empty()) {
        /// Add remapped files, if bounds is provided, cut off the content.
        std::size_t size = params.bound.has_value() ? params.bound.value() : params.content.size();
        PPOpts.addRemappedFile(
            params.srcPath,
            llvm::MemoryBuffer::getMemBufferCopy(params.content.substr(0, size), params.srcPath)
                .release());
    }

    for(auto& [file, buffer]: params.buffers) {
        PPOpts.addRemappedFile(file, buffer.release());
    }
    params.buffers.clear();

    assert(!PPOpts.RetainRemappedFileBuffers && "RetainRemappedFileBuffers should be false");

    auto [pch, bound] = params.pch;
    PPOpts.ImplicitPCHInclude = std::move(pch);
    if(bound != 0) {
        PPOpts.PrecompiledPreambleBytes = {bound, false};
    }

    auto& HSOpts = invocation->getHeaderSearchOpts();
    for(auto& [name, path]: params.pcms) {
        HSOpts.PrebuiltModuleFiles.try_emplace(name.str(), std::move(path));
    }

    return invocation;
}

template <typename Action, typename Adjuster>
std::expected<CompilationUnit, std::string> clang_compile(CompilationParams& params,
                                                          const Adjuster& adjuster) {
    auto diagnostics = std::make_shared<std::vector<Diagnostic>>();
    auto diagnostic_engine =
        clang::CompilerInstance::createDiagnostics(*params.vfs,
                                                   new clang::DiagnosticOptions(),
                                                   Diagnostic::create(diagnostics));

    auto invocation = createInvocation(params, diagnostics, diagnostic_engine);
    TRY_OR_RETURN(invocation);

    auto instance = std::make_unique<clang::CompilerInstance>();
    instance->setInvocation(std::move(*invocation));
    instance->setDiagnostics(diagnostic_engine.get());

    if(auto remapping = clang::createVFSFromCompilerInvocation(instance->getInvocation(),
                                                               instance->getDiagnostics(),
                                                               params.vfs)) {
        instance->createFileManager(std::move(remapping));
    }

    if(!instance->createTarget()) {
        return std::unexpected("fail to create target");
    }

    /// Adjust the compiler instance, for example, set preamble or modules.
    adjuster(*instance);

    auto action = std::make_unique<Action>();

    if(!action->BeginSourceFile(*instance, instance->getFrontendOpts().Inputs[0])) {
        /// TODO: collect error message from diagnostics.
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
    return clang_compile<clang::PreprocessOnlyAction>(params, [](auto&) {});
}

std::expected<CompilationUnit, std::string> compile(CompilationParams& params) {
    return clang_compile<clang::SyntaxOnlyAction>(params, [](auto&) {});
}

std::expected<CompilationUnit, std::string> complete(CompilationParams& params,
                                                     clang::CodeCompleteConsumer* consumer) {

    auto& [file, offset] = params.completion;
    assert(file == params.srcPath && "completing could only occur in main file");

    /// The location of clang is 1-1 based.
    std::uint32_t line = 1;
    std::uint32_t column = 1;
    llvm::StringRef content = params.content;

    for(auto c: content.substr(0, offset)) {
        if(c == '\n') {
            line += 1;
            column = 1;
            continue;
        }
        column += 1;
    }

    return clang_compile<clang::SyntaxOnlyAction>(params, [&](clang::CompilerInstance& instance) {
        /// Set options to run code completion.
        instance.getFrontendOpts().CodeCompletionAt.FileName = std::move(file);
        instance.getFrontendOpts().CodeCompletionAt.Line = line;
        instance.getFrontendOpts().CodeCompletionAt.Column = column;
        instance.setCodeCompletionConsumer(consumer);
    });
}

std::expected<CompilationUnit, std::string> compile(CompilationParams& params, PCHInfo& out) {
    assert(params.bound.has_value() && "Preamble bounds is required to build PCH");

    out.path = params.outPath.str();
    out.preamble = params.content.substr(0, *params.bound);
    out.command = params.command.str();
    /// FIXME: out.deps = info->deps();

    return clang_compile<clang::GeneratePCHAction>(params, [&](clang::CompilerInstance& instance) {
        /// Set options to generate PCH.
        instance.getFrontendOpts().OutputFile = params.outPath.str();
        instance.getFrontendOpts().ProgramAction = clang::frontend::GeneratePCH;
        instance.getPreprocessorOpts().GeneratePreamble = true;
        instance.getLangOpts().CompilingPCH = true;
    });
}

std::expected<CompilationUnit, std::string> compile(CompilationParams& params, PCMInfo& out) {
    for(auto& [name, path]: params.pcms) {
        out.mods.emplace_back(name);
    }
    out.path = params.outPath.str();
    out.srcPath = params.srcPath.str();

    return clang_compile<clang::GenerateReducedModuleInterfaceAction>(
        params,
        [&](clang::CompilerInstance& instance) {
            /// Set options to generate PCH.
            instance.getFrontendOpts().OutputFile = params.outPath.str();
            instance.getFrontendOpts().ProgramAction =
                clang::frontend::GenerateReducedModuleInterface;
        });
}

}  // namespace clice
