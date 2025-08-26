//===--- Run clang-tidy ---------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

/// Partial code is copied from clangd. See:
/// https://github.com/llvm/llvm-project//blob/0865ecc5150b9a55ba1f9e30b6d463a66ac362a6/clang-tools-extra/clangd/ParsedAST.cpp#L547
/// https://github.com/llvm/llvm-project//blob/0865ecc5150b9a55ba1f9e30b6d463a66ac362a6/clang-tools-extra/clangd/TidyProvider.cpp

#include "clang-tidy/ClangTidyModuleRegistry.h"
#include "clang-tidy/ClangTidyOptions.h"

#include "llvm/ADT/StringSet.h"
#include "llvm/Support/Allocator.h"

#include "Server/Tidy.h"

namespace clice::tidy {

using namespace clang::tidy;

bool isRegisteredTidyCheck(llvm::StringRef Check) {
    assert(!Check.empty());
    assert(!Check.contains('*') && !Check.contains(',') &&
           "isRegisteredCheck doesn't support globs");
    assert(Check.ltrim().front() != '-');

    const static llvm::StringSet<llvm::BumpPtrAllocator> AllChecks = [] {
        llvm::StringSet<llvm::BumpPtrAllocator> Result;
        tidy::ClangTidyCheckFactories Factories;
        for(tidy::ClangTidyModuleRegistry::entry E: tidy::ClangTidyModuleRegistry::entries())
            E.instantiate()->addCheckFactories(Factories);
        for(const auto& Factory: Factories)
            Result.insert(Factory.getKey());
        return Result;
    }();

    return AllChecks.contains(Check);
}

std::optional<bool> isFastTidyCheck(llvm::StringRef Check) {
    static auto& Fast = *new llvm::StringMap<bool>{
#define FAST(CHECK, TIME) {#CHECK, true},
#define SLOW(CHECK, TIME) {#CHECK, false},
// todo: move me to llvm toolchain headers.
#include "TidyFastChecks.inc"
    };
    if(auto It = Fast.find(Check); It != Fast.end())
        return It->second;
    return std::nullopt;
}

TidyProvider provideDefaultChecks() {
    // These default checks are chosen for:
    //  - low false-positive rate
    //  - providing a lot of value
    //  - being reasonably efficient
    const static std::string DefaultChecks = llvm::join_items(",",
                                                              "readability-misleading-indentation",
                                                              "readability-deleted-default",
                                                              "bugprone-integer-division",
                                                              "bugprone-sizeof-expression",
                                                              "bugprone-suspicious-missing-comma",
                                                              "bugprone-unused-raii",
                                                              "bugprone-unused-return-value",
                                                              "misc-unused-using-decls",
                                                              "misc-unused-alias-decls",
                                                              "misc-definitions-in-headers");
    return [](tidy::ClangTidyOptions& Opts, llvm::StringRef) {
        if(!Opts.Checks || Opts.Checks->empty())
            Opts.Checks = DefaultChecks;
    };
}

TidyProvider addTidyChecks(llvm::StringRef Checks, llvm::StringRef WarningsAsErrors) {
    return [Checks = std::string(Checks),
            WarningsAsErrors = std::string(WarningsAsErrors)](tidy::ClangTidyOptions& Opts,
                                                              llvm::StringRef) {
        mergeCheckList(Opts.Checks, Checks);
        mergeCheckList(Opts.WarningsAsErrors, WarningsAsErrors);
    };
}

TidyProvider disableUnusableChecks(llvm::ArrayRef<std::string> ExtraBadChecks) {
    constexpr llvm::StringLiteral Separator(",");
    const static std::string BadChecks =
        llvm::join_items(Separator,
                         // We want this list to start with a separator to
                         // simplify appending in the lambda. So including an
                         // empty string here will force that.
                         "",
                         // include-cleaner is directly integrated in IncludeCleaner.cpp
                         "-misc-include-cleaner",

                         // ----- False Positives -----

                         // Check relies on seeing ifndef/define/endif directives,
                         // clangd doesn't replay those when using a preamble.
                         "-llvm-header-guard",
                         "-modernize-macro-to-enum",

                         // ----- Crashing Checks -----

                         // Check can choke on invalid (intermediate) c++
                         // code, which is often the case when clangd
                         // tries to build an AST.
                         "-bugprone-use-after-move",
                         // Alias for bugprone-use-after-move.
                         "-hicpp-invalid-access-moved",
                         // Check uses dataflow analysis, which might hang/crash unexpectedly on
                         // incomplete code.
                         "-bugprone-unchecked-optional-access");

    size_t Size = BadChecks.size();
    for(const std::string& Str: ExtraBadChecks) {
        if(Str.empty())
            continue;
        Size += Separator.size();
        if(LLVM_LIKELY(Str.front() != '-'))
            ++Size;
        Size += Str.size();
    }
    std::string DisableGlob;
    DisableGlob.reserve(Size);
    DisableGlob += BadChecks;
    for(const std::string& Str: ExtraBadChecks) {
        if(Str.empty())
            continue;
        DisableGlob += Separator;
        if(LLVM_LIKELY(Str.front() != '-'))
            DisableGlob.push_back('-');
        DisableGlob += Str;
    }

    return [DisableList(std::move(DisableGlob))](tidy::ClangTidyOptions& Opts, llvm::StringRef) {
        if(Opts.Checks && !Opts.Checks->empty())
            Opts.Checks->append(DisableList);
    };
}

TidyProvider provideEnvironment() {
    const static std::optional<std::string> User = [] {
        std::optional<std::string> Ret = llvm::sys::Process::GetEnv("USER");
#ifdef _WIN32
        if(!Ret)
            return llvm::sys::Process::GetEnv("USERNAME");
#endif
        return Ret;
    }();

    if(User)
        return [](tidy::ClangTidyOptions& Opts, llvm::StringRef) {
            Opts.User = User;
        };
    // FIXME: Once function_ref and unique_function operator= operators handle
    // null values, this can return null.
    return [](tidy::ClangTidyOptions&, llvm::StringRef) {
    };
}

TidyProvider provideClangdConfig() {
    return [](tidy::ClangTidyOptions& Opts, llvm::StringRef) {
        const auto& CurTidyConfig = Config::current().Diagnostics.ClangTidy;
        if(!CurTidyConfig.Checks.empty())
            mergeCheckList(Opts.Checks, CurTidyConfig.Checks);

        for(const auto& CheckOption: CurTidyConfig.CheckOptions)
            Opts.CheckOptions.insert_or_assign(
                CheckOption.getKey(),
                tidy::ClangTidyOptions::ClangTidyValue(CheckOption.getValue(), 10000U));
    };
}

TidyProvider combine(std::vector<TidyProvider> Providers) {
    // FIXME: Once function_ref and unique_function operator= operators handle
    // null values, we should filter out any Providers that are null. Right now we
    // have to ensure we dont pass any providers that are null.
    return
        [Providers(std::move(Providers))](tidy::ClangTidyOptions& Opts, llvm::StringRef Filename) {
            for(const auto& Provider: Providers)
                Provider(Opts, Filename);
        };
}

// enum class FastCheckPolicy { Strict, Loose, None };
// FastCheckPolicy fast_check_filter = FastCheckPolicy::Strict;

tidy::ClangTidyOptions getTidyOptionsForFile(TidyProviderRef Provider, llvm::StringRef Filename) {
    /// getDefaults instantiates all check factories, which are registered at link
    /// time. So cache the results once.
    const static auto* DefaultOpts = [] {
        auto* Opts = new tidy::ClangTidyOptions;
        *Opts = tidy::ClangTidyOptions::getDefaults();
        Opts->Checks->clear();
        return Opts;
    }();
    auto Opts = *DefaultOpts;
    if(Provider)
        Provider(Opts, Filename);
    return Opts;
}

void check() {
    /// No need to run clang-tidy or IncludeFixerif we are not going to surface
    /// diagnostics.
    const static auto* AllCTFactories = [] {
        auto* CTFactories = new tidy::ClangTidyCheckFactories;
        for(const auto& E: tidy::ClangTidyModuleRegistry::entries())
            E.instantiate()->addCheckFactories(*CTFactories);
        return CTFactories;
    }();
    tidy::ClangTidyCheckFactories FastFactories =
        filterFastTidyChecks(*AllCTFactories, Cfg.Diagnostics.ClangTidy.FastCheckFilter);
    CTContext.emplace(std::make_unique<tidy::DefaultOptionsProvider>(tidy::ClangTidyGlobalOptions(),
                                                                     ClangTidyOpts));
    CTContext->setDiagnosticsEngine(&Clang->getDiagnostics());
    CTContext->setASTContext(&Clang->getASTContext());
    CTContext->setCurrentFile(Filename);
    CTContext->setSelfContainedDiags(true);
    CTChecks = FastFactories.createChecksForLanguage(&*CTContext);
    Preprocessor* PP = &Clang->getPreprocessor();
    for(const auto& Check: CTChecks) {
        Check->registerPPCallbacks(Clang->getSourceManager(), PP, PP);
        Check->registerMatchers(&CTFinder);
    }

    // Clang only corrects typos for use of undeclared functions in C if that
    // use is an error. Include fixer relies on typo correction, so pretend
    // this is an error. (The actual typo correction is nice too).
    // We restore the original severity in the level adjuster.
    // FIXME: It would be better to have a real API for this, but what?
    for(auto ID: {diag::ext_implicit_function_decl_c99,
                  diag::ext_implicit_lib_function_decl,
                  diag::ext_implicit_lib_function_decl_c99,
                  diag::warn_implicit_function_decl}) {
        OverriddenSeverity.try_emplace(
            ID,
            Clang->getDiagnostics().getDiagnosticLevel(ID, SourceLocation()));
        Clang->getDiagnostics().setSeverity(ID, diag::Severity::Error, SourceLocation());
    }

    ASTDiags.setLevelAdjuster([&](DiagnosticsEngine::Level DiagLevel,
                                  const clang::Diagnostic& Info) {
        if(Cfg.Diagnostics.SuppressAll ||
           isDiagnosticSuppressed(Info, Cfg.Diagnostics.Suppress, Clang->getLangOpts()))
            return DiagnosticsEngine::Ignored;

        auto It = OverriddenSeverity.find(Info.getID());
        if(It != OverriddenSeverity.end())
            DiagLevel = It->second;

        if(!CTChecks.empty()) {
            std::string CheckName = CTContext->getCheckName(Info.getID());
            bool IsClangTidyDiag = !CheckName.empty();
            if(IsClangTidyDiag) {
                if(Cfg.Diagnostics.Suppress.contains(CheckName))
                    return DiagnosticsEngine::Ignored;
                // Check for suppression comment. Skip the check for diagnostics not
                // in the main file, because we don't want that function to query the
                // source buffer for preamble files. For the same reason, we ask
                // shouldSuppressDiagnostic to avoid I/O.
                // We let suppression comments take precedence over warning-as-error
                // to match clang-tidy's behaviour.
                bool IsInsideMainFile =
                    Info.hasSourceManager() &&
                    isInsideMainFile(Info.getLocation(), Info.getSourceManager());
                SmallVector<tooling::Diagnostic, 1> TidySuppressedErrors;
                if(IsInsideMainFile &&
                   CTContext->shouldSuppressDiagnostic(DiagLevel,
                                                       Info,
                                                       TidySuppressedErrors,
                                                       /*AllowIO=*/false,
                                                       /*EnableNolintBlocks=*/true)) {
                    // FIXME: should we expose the suppression error (invalid use of
                    // NOLINT comments)?
                    return DiagnosticsEngine::Ignored;
                }
                if(!CTContext->getOptions().SystemHeaders.value_or(false) &&
                   Info.hasSourceManager() &&
                   Info.getSourceManager().isInSystemMacro(Info.getLocation()))
                    return DiagnosticsEngine::Ignored;

                // Check for warning-as-error.
                if(DiagLevel == DiagnosticsEngine::Warning && CTContext->treatAsError(CheckName)) {
                    return DiagnosticsEngine::Error;
                }
            }
        }
        return DiagLevel;
    });

    // Add IncludeFixer which can recover diagnostics caused by missing includes
    // (e.g. incomplete type) and attach include insertion fixes to diagnostics.
    if(Inputs.Index && !BuildDir.getError()) {
        auto Style = getFormatStyleForFile(Filename, Inputs.Contents, *Inputs.TFS, false);
        auto Inserter =
            std::make_shared<IncludeInserter>(Filename,
                                              Inputs.Contents,
                                              Style,
                                              BuildDir.get(),
                                              &Clang->getPreprocessor().getHeaderSearchInfo(),
                                              Cfg.Style.QuotedHeaders,
                                              Cfg.Style.AngledHeaders);
        ArrayRef<Inclusion> MainFileIncludes;
        if(Preamble) {
            MainFileIncludes = Preamble->Includes.MainFileIncludes;
            for(const auto& Inc: Preamble->Includes.MainFileIncludes)
                Inserter->addExisting(Inc);
        }
        // FIXME: Consider piping through ASTSignals to fetch this to handle the
        // case where a header file contains ObjC decls but no #imports.
        Symbol::IncludeDirective Directive =
            Inputs.Opts.ImportInsertions
                ? preferredIncludeDirective(Filename, Clang->getLangOpts(), MainFileIncludes, {})
                : Symbol::Include;
        FixIncludes.emplace(Filename,
                            Inserter,
                            *Inputs.Index,
                            /*IndexRequestLimit=*/5,
                            Directive);
        ASTDiags.contributeFixes(
            [&FixIncludes](DiagnosticsEngine::Level DiagLevl, const clang::Diagnostic& Info) {
                return FixIncludes->fix(DiagLevl, Info);
            });
        Clang->setExternalSemaSource(FixIncludes->unresolvedNameRecorder());
    }
}

}  // namespace clice::tidy
