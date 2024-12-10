#include <Compiler/Command.h>
#include <Compiler/Compiler.h>

#include <clang/Lex/PreprocessorOptions.h>
#include <clang/Frontend/TextDiagnosticPrinter.h>

namespace clice {

bool PCHInfo::needUpdate(llvm::StringRef content) {
    auto size = this->bounds().Size;
    if(content.substr(0, size) != preamble.substr(0, size)) {
        return true;
    }

    /// FIXME: check timestamp of all files involved in building this PCH.

    return false;
}

namespace {

auto createInvocation(CompliationParams& params) {
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

auto createInstance(CompliationParams& params) {
    auto instance = std::make_unique<clang::CompilerInstance>();

    instance->setInvocation(createInvocation(params));

    /// TODO: use a thread safe filesystem and our customized `DiagnosticConsumer`.
    instance->createDiagnostics(
        *params.vfs,
        new clang::TextDiagnosticPrinter(llvm::outs(), new clang::DiagnosticOptions()),
        true);

    instance->createFileManager(params.vfs);

    /// Add remapped files, if bounds is provided, cut off the content.
    std::size_t size =
        params.bounds.has_value() ? params.bounds.value().Size : params.content.size();

    assert(!instance->getPreprocessorOpts().RetainRemappedFileBuffers &&
           "RetainRemappedFileBuffers should be false");
    instance->getPreprocessorOpts().addRemappedFile(
        params.srcPath,
        llvm::MemoryBuffer::getMemBufferCopy(params.content.substr(0, size), params.srcPath)
            .release());

    for(auto& [file, content]: params.remappedFiles) {
        instance->getPreprocessorOpts().addRemappedFile(
            file,
            llvm::MemoryBuffer::getMemBufferCopy(content, file).release());
    }

    return instance;
}

void applyPreamble(clang::CompilerInstance& instance, CompliationParams& params) {
    auto& PPOpts = instance.getPreprocessorOpts();
    auto& pch = params.pch;
    auto& bounds = params.pchBounds;
    auto& pcms = params.pcms;
    if(bounds.Size != 0) {
        PPOpts.UsePredefines = false;
        PPOpts.ImplicitPCHInclude = std::move(pch);
        PPOpts.PrecompiledPreambleBytes.first = bounds.Size;
        PPOpts.PrecompiledPreambleBytes.second = bounds.PreambleEndsAtStartOfLine;
        PPOpts.DisablePCHOrModuleValidation = clang::DisableValidationForModuleKind::PCH;
    }

    for(auto& [name, path]: pcms) {
        auto& HSOpts = instance.getHeaderSearchOpts();
        HSOpts.PrebuiltModuleFiles.try_emplace(std::move(name), std::move(path));
    }
}

llvm::Expected<ASTInfo> ExecuteAction(std::unique_ptr<clang::CompilerInstance> instance,
                                      clang::frontend::ActionKind kind) {
    std::unique_ptr<clang::ASTFrontendAction> action;
    if(kind == clang::frontend::ActionKind::ParseSyntaxOnly) {
        action = std::make_unique<clang::SyntaxOnlyAction>();
    } else if(kind == clang::frontend::ActionKind::GeneratePCH) {
        action = std::make_unique<clang::GeneratePCHAction>();
    } else if(kind == clang::frontend::ActionKind::GenerateReducedModuleInterface) {
        action = std::make_unique<clang::GenerateReducedModuleInterfaceAction>();
    } else {
        llvm::errs() << "Unsupported action kind\n";
        std::terminate();
    }

    if(!instance->createTarget()) {
        return error("Failed to create target");
    }

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

    std::optional<clang::syntax::TokenCollector> collector;

    /// It is not necessary to collect tokens if we are running code completion.
    /// And in fact will cause assertion failure.
    if(!instance->hasCodeCompletionConsumer()) {
        collector.emplace(pp);
    }

    if(auto error = action->Execute()) {
        return clice::error("Failed to execute action, because {} ", error);
    }

    std::unique_ptr<clang::syntax::TokenBuffer> tokBuf;
    if(collector) {
        tokBuf = std::make_unique<clang::syntax::TokenBuffer>(std::move(*collector).consume());
    }

    return ASTInfo(std::move(action),
                   std::move(instance),
                   std::move(tokBuf),
                   std::move(directives));
}

}  // namespace

void CompliationParams::computeBounds(llvm::StringRef header) {
    assert(!bounds.has_value() && "Bounds is already computed");
    assert(!content.empty() && "Source content is required to compute bounds");

    if(header.empty()) {
        auto invocation = createInvocation(*this);
        bounds = clang::Lexer::ComputePreamble(content, invocation->getLangOpts());
        return;
    }

    auto instance = createInstance(*this);

    instance->getFrontendOpts().ProgramAction = clang::frontend::RunPreprocessorOnly;

    struct SearchBoundary : public clang::PPCallbacks {
        llvm::StringRef header;
        clang::SourceLocation& hashLoc;

        SearchBoundary(llvm::StringRef header, clang::SourceLocation& hashLoc) :
            header(header), hashLoc(hashLoc) {}

        void InclusionDirective(clang::SourceLocation hashLoc,
                                const clang::Token& includeTok,
                                llvm::StringRef filename,
                                bool isAngled,
                                clang::CharSourceRange filenameRange,
                                clang::OptionalFileEntryRef file,
                                llvm::StringRef searchPath,
                                llvm::StringRef relativePath,
                                const clang::Module* suggestedModule,
                                bool moduleImported,
                                clang::SrcMgr::CharacteristicKind fileType) override {
            llvm::SmallString<128> path;
            if(searchPath != ".") {
                path::append(path, searchPath);
            }
            path::append(path, relativePath);
            if(path == header) {
                this->hashLoc = hashLoc;
            }
        }
    };

    /// The hash location of the include directive that includes the header.
    clang::SourceLocation hashLoc;

    clang::PreprocessOnlyAction action;

    /// FIXME: merge the logic to `ExecuteAction`.
    if(!instance->createTarget()) {
        llvm::errs() << "Failed to create target\n";
        std::terminate();
    }

    if(!action.BeginSourceFile(*instance, instance->getFrontendOpts().Inputs[0])) {
        llvm::errs() << "Failed to begin source file\n";
        std::terminate();
    }

    instance->getPreprocessor().addPPCallbacks(std::make_unique<SearchBoundary>(header, hashLoc));

    if(auto error = action.Execute()) {
        llvm::errs() << "Failed to execute action, because " << error << "\n";
        std::terminate();
    }

    if(hashLoc.isInvalid()) {
        llvm::errs() << "Failed to find the boundary\n";
        std::terminate();
    }

    /// Find the top level file.
    auto& srcMgr = instance->getSourceManager();
    while(srcMgr.getIncludeLoc(srcMgr.getFileID(hashLoc)).isValid()) {
        hashLoc = srcMgr.getIncludeLoc(srcMgr.getFileID(hashLoc));
    }
    auto offset = srcMgr.getFileOffset(hashLoc);

    action.EndSourceFile();

    /// We need to move to next line to get the correct bounds.
    for(auto i = offset; i < content.size(); ++i) {
        if(content[i] == '\n') {
            bounds = {i + 2, true};
            break;
        }
    }

    if(!bounds.has_value()) {
        llvm::errs() << "Failed to compute bounds\n";
        std::terminate();
    }
}

llvm::Expected<ASTInfo> compile(CompliationParams& params) {
    auto instance = createInstance(params);

    applyPreamble(*instance, params);

    return ExecuteAction(std::move(instance), clang::frontend::ActionKind::ParseSyntaxOnly);
}

llvm::Expected<ASTInfo> compile(CompliationParams& params, PCHInfo& out) {
    assert(params.bounds.has_value() && "Preamble bounds is required to build PCH");

    auto instance = createInstance(params);

    /// Set options to generate PCH.
    instance->getFrontendOpts().OutputFile = params.outPath.str();
    instance->getFrontendOpts().ProgramAction = clang::frontend::GeneratePCH;
    instance->getPreprocessorOpts().PrecompiledPreambleBytes = {0, false};
    instance->getPreprocessorOpts().GeneratePreamble = true;
    instance->getLangOpts().CompilingPCH = true;

    if(auto info = ExecuteAction(std::move(instance), clang::frontend::ActionKind::GeneratePCH)) {
        out.path = params.outPath.str();
        out.srcPath = params.srcPath.str();

        auto& bounds = *params.bounds;
        out.preamble = params.content.substr(0, bounds.Size).str();
        if(bounds.PreambleEndsAtStartOfLine) {
            out.preamble.append("@");
        }

        /// TODO: collect files involved in building this PCH.

        return std::move(*info);
    } else {
        return info.takeError();
    }
}

llvm::Expected<ASTInfo> compile(CompliationParams& params, PCMInfo& out) {
    auto instance = createInstance(params);

    /// Set options to generate PCM.
    instance->getFrontendOpts().OutputFile = params.outPath.str();
    instance->getFrontendOpts().ProgramAction = clang::frontend::GenerateReducedModuleInterface;

    applyPreamble(*instance, params);

    if(auto info = ExecuteAction(std::move(instance),
                                 clang::frontend::ActionKind::GenerateReducedModuleInterface)) {
        out.path = params.outPath.str();
        out.name = info->context().getCurrentNamedModule()->Name;

        return std::move(*info);
    } else {
        return info.takeError();
    }
}

llvm::Expected<ASTInfo> compile(CompliationParams& params, clang::CodeCompleteConsumer* consumer) {
    auto instance = createInstance(params);

    /// Set options to run code completion.
    instance->getFrontendOpts().CodeCompletionAt.FileName = params.srcPath.str();
    instance->getFrontendOpts().CodeCompletionAt.Line = params.line;
    instance->getFrontendOpts().CodeCompletionAt.Column = params.column;
    instance->setCodeCompletionConsumer(consumer);

    applyPreamble(*instance, params);

    return ExecuteAction(std::move(instance), clang::frontend::ActionKind::ParseSyntaxOnly);
}

}  // namespace clice
