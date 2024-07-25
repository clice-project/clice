#include <Clang/Compiler.h>
#include <clang/Lex/PreprocessorOptions.h>

namespace clice {

void setOptions(CompilerInvocation& invocation) {
    // TODO: add comments for reason of setting these options
    clang::DiagnosticOptions& diagnostic = invocation.getDiagnosticOpts();
    diagnostic.VerifyDiagnostics = false;
    diagnostic.ShowColors = false;

    clang::DependencyOutputOptions& dependency = invocation.getDependencyOutputOpts();
    dependency.ShowIncludesDest = clang::ShowIncludesDestination::None;
    dependency.OutputFile.clear();
    dependency.HeaderIncludeOutputFile.clear();
    dependency.DOTOutputFile.clear();
    dependency.ModuleDependencyOutputDir.clear();

    clang::PreprocessorOptions& preprocessor = invocation.getPreprocessorOpts();
    preprocessor.ImplicitPCHInclude.clear();
    preprocessor.PrecompiledPreambleBytes = {0, false};
    preprocessor.PCHThroughHeader.clear();
    preprocessor.PCHWithHdrStop = false;
    preprocessor.PCHWithHdrStopCreate = false;

    clang::FrontendOptions& frontend = invocation.getFrontendOpts();
    frontend.DisableFree = false;
    frontend.Plugins.clear();
    frontend.PluginArgs.clear();
    frontend.AddPluginActions.clear();
    frontend.ActionName.clear();
    frontend.ProgramAction = clang::frontend::ActionKind::ParseSyntaxOnly;

    clang::LangOptions& lang = invocation.getLangOpts();
    lang.CommentOpts.ParseAllComments = true;
    lang.RetainCommentsFromSystemHeaders = true;
    lang.NoSanitizeFiles.clear();
    lang.ProfileListFiles.clear();
    lang.XRayAttrListFiles.clear();
    lang.XRayAlwaysInstrumentFiles.clear();
    lang.XRayNeverInstrumentFiles.clear();
}

auto createCompilerInvocation(clang::tooling::CompileCommand& command,
                              std::vector<std::string>* extraArgs,
                              llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem> vfs,
                              clang::DiagnosticConsumer& consumer) -> std::unique_ptr<CompilerInvocation> {
    clang::CreateInvocationOptions options;
    options.VFS = vfs;
    options.CC1Args = extraArgs;
    options.RecoverOnError = true;
    options.Diags = CompilerInstance::createDiagnostics(new clang::DiagnosticOptions(), &consumer, false);
    options.ProbePrecompiled = false;

    llvm::ArrayRef<std::string> argv = command.CommandLine;
    assert(argv.size() > 0 && "command line is empty");

    std::vector<const char*> args;
    args.reserve(argv.size() + 1);
    args = {argv.front().c_str(), "-Xclang", "-no-round-trip-args"};
    // TODO: add comment for why do this
    for(const auto& arg: argv.drop_front()) {
        args.push_back(arg.c_str());
    }

    auto invocation = clang::createInvocation(args, options);
    assert(invocation && "failed to create invocation");

    setOptions(*invocation);
    return invocation;
}

auto createCompilerInstance(std::unique_ptr<CompilerInvocation> invocation,
                            const clang::PrecompiledPreamble* preamble,
                            std::unique_ptr<llvm::MemoryBuffer> content,
                            llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem> vfs,
                            clang::DiagnosticConsumer& consumer) -> std::unique_ptr<CompilerInstance> {
    assert(vfs && "vfs cannot be null");

    if(preamble) {
        auto bounds = clang::ComputePreambleBounds(invocation->getLangOpts(), *content, false);
        if(preamble->CanReuse(*invocation, *content, bounds, *vfs)) {
            preamble->AddImplicitPreamble(*invocation, vfs, content.release());
        } else {
            // TODO:
        }
    }

    auto instance = std::make_unique<CompilerInstance>(std::make_shared<clang::PCHContainerOperations>());
    instance->setInvocation(std::move(invocation));
    instance->createDiagnostics(&consumer, false);

    if(auto remappedVFS = clang::createVFSFromCompilerInvocation(instance->getInvocation(),
                                                                 instance->getDiagnostics(),
                                                                 vfs)) {
        vfs = remappedVFS;
    }
    instance->createFileManager(vfs);

    if(!instance->createTarget()) {
        // TODO: report an error
    }

    return instance;
}

}  // namespace clice

