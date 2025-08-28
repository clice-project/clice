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

#include "Support/Logger.h"
#include "clang-tidy/ClangTidyModuleRegistry.h"
#include "clang-tidy/ClangTidyOptions.h"
#include "clang-tidy/ClangTidyCheck.h"
#include "clang-tidy/ClangTidyDiagnosticConsumer.h"

#include "llvm/ADT/StringSet.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Support/Allocator.h"
#include "llvm/Support/Process.h"

#include "clang/Frontend/CompilerInstance.h"
#include <llvm/Support/StringSaver.h>

#include "Compiler/Diagnostic.h"
#include "Compiler/Tidy.h"

#include "TidyImpl.h"

namespace clice::tidy {

using namespace clang::tidy;

bool is_registered_tidy_check(llvm::StringRef check) {
    assert(!check.empty());
    assert(!check.contains('*') && !check.contains(',') &&
           "isRegisteredCheck doesn't support globs");
    assert(check.ltrim().front() != '-');

    const static llvm::StringSet<llvm::BumpPtrAllocator> all_checks = [] {
        llvm::StringSet<llvm::BumpPtrAllocator> result;
        tidy::ClangTidyCheckFactories factories;
        for(tidy::ClangTidyModuleRegistry::entry entry: tidy::ClangTidyModuleRegistry::entries())
            entry.instantiate()->addCheckFactories(factories);
        for(const auto& factory: factories)
            result.insert(factory.getKey());
        return result;
    }();

    return all_checks.contains(check);
}

std::optional<bool> is_fast_tidy_check(llvm::StringRef check) {
    static auto& fast = *new llvm::StringMap<bool>{
#define FAST(CHECK, TIME) {#CHECK, true},
#define SLOW(CHECK, TIME) {#CHECK, false},
// todo: move me to llvm toolchain headers.
#include "TidyFastChecks.inc"
    };
    if(auto it = fast.find(check); it != fast.end())
        return it->second;
    return std::nullopt;
}

tidy::ClangTidyCheckFactories filterFastTidyChecks(const tidy::ClangTidyCheckFactories& All) {
    tidy::ClangTidyCheckFactories Fast;
    for(const auto& Factory: All) {
        if(isFastTidyCheck(Factory.getKey()).value_or(false))
            Fast.registerCheckFactory(Factory.first(), Factory.second);
    }
    return Fast;
}

tidy::ClangTidyOptions createTidyOptions() {
    // getDefaults instantiates all check factories, which are registered at link
    // time. So cache the results once.
    const static auto* DefaultOpts = [] {
        auto* Opts = new tidy::ClangTidyOptions;
        *Opts = tidy::ClangTidyOptions::getDefaults();
        Opts->Checks->clear();
        return Opts;
    }();
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
    const static std::string BadChecks =
        llvm::join_items(",",
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

    tidy::ClangTidyOptions opts = *DefaultOpts;

    opts.Checks->clear();
    // clang::clangd::provideEnvironment
    if(std::optional<std::string> user = llvm::sys::Process::GetEnv("USER"))
        opts.User = user;
    // TODO: Providers.push_back(provideClangTidyFiles(TFS)); Filename
    // TODO: if(EnableConfig) Providers.push_back(provideClangdConfig());
    // clang::clangd::provideDefaultChecks
    if(!opts.Checks || opts.Checks->empty())
        opts.Checks = DefaultChecks;
    // clang::clangd::disableUnusableChecks
    if(opts.Checks && !opts.Checks->empty())
        opts.Checks->append(BadChecks);
    return opts;
}

// Filter for clang diagnostics groups enabled by CTOptions.Checks.
//
// These are check names like clang-diagnostics-unused.
// Note that unlike -Wunused, clang-diagnostics-unused does not imply
// subcategories like clang-diagnostics-unused-function.
//
// This is used to determine which diagnostics can be enabled by ExtraArgs in
// the clang-tidy configuration.
class TidyDiagnosticGroups {
    // Whether all diagnostic groups are enabled by default.
    // True if we've seen clang-diagnostic-*.
    bool Default = false;
    // Set of diag::Group whose enablement != Default.
    // If Default is false, this is foo where we've seen clang-diagnostic-foo.
    llvm::DenseSet<unsigned> Exceptions;

public:
    TidyDiagnosticGroups(llvm::StringRef Checks) {
        constexpr llvm::StringLiteral CDPrefix = "clang-diagnostic-";

        llvm::StringRef Check;
        while(!Checks.empty()) {
            std::tie(Check, Checks) = Checks.split(',');
            Check = Check.trim();

            if(Check.empty())
                continue;

            bool Enable = !Check.consume_front("-");
            bool Glob = Check.consume_back("*");
            if(Glob) {
                // Is this clang-diagnostic-*, or *, or so?
                // (We ignore all other types of globs).
                if(CDPrefix.starts_with(Check)) {
                    Default = Enable;
                    Exceptions.clear();
                }
                continue;
            }

            // In "*,clang-diagnostic-foo", the latter is a no-op.
            if(Default == Enable)
                continue;
            // The only non-glob entries we care about are clang-diagnostic-foo.
            if(!Check.consume_front(CDPrefix))
                continue;

            if(auto Group = clang::DiagnosticIDs::getGroupForWarningOption(Check))
                Exceptions.insert(static_cast<unsigned>(*Group));
        }
    }

    bool operator() (clang::diag::Group GroupID) const {
        return Exceptions.contains(static_cast<unsigned>(GroupID)) ? !Default : Default;
    }
};

// Find -W<group> and -Wno-<group> options in ExtraArgs and apply them to Diags.
//
// This is used to handle ExtraArgs in clang-tidy configuration.
// We don't use clang's standard handling of this as we want slightly different
// behavior (e.g. we want to exclude these from -Wno-error).
void applyWarningOptions(llvm::ArrayRef<std::string> ExtraArgs,
                         llvm::function_ref<bool(clang::diag::Group)> EnabledGroups,
                         clang::DiagnosticsEngine& Diags) {
    for(llvm::StringRef Group: ExtraArgs) {
        // Only handle args that are of the form -W[no-]<group>.
        // Other flags are possible but rare and deliberately out of scope.
        llvm::SmallVector<clang::diag::kind> Members;
        if(!Group.consume_front("-W") || Group.empty())
            continue;
        bool Enable = !Group.consume_front("no-");
        if(Diags.getDiagnosticIDs()->getDiagnosticsInGroup(clang::diag::Flavor::WarningOrError,
                                                           Group,
                                                           Members))
            continue;

        // Upgrade (or downgrade) the severity of each diagnostic in the group.
        // If -Werror is on, newly added warnings will be treated as errors.
        // We don't want this, so keep track of them to fix afterwards.
        bool NeedsWerrorExclusion = false;
        for(clang::diag::kind ID: Members) {
            if(Enable) {
                if(Diags.getDiagnosticLevel(ID, clang::SourceLocation()) <
                   clang::DiagnosticsEngine::Warning) {
                    auto Group = Diags.getDiagnosticIDs()->getGroupForDiag(ID);
                    if(!Group || !EnabledGroups(*Group))
                        continue;
                    Diags.setSeverity(ID, clang::diag::Severity::Warning, clang::SourceLocation());
                    if(Diags.getWarningsAsErrors())
                        NeedsWerrorExclusion = true;
                }
            } else {
                Diags.setSeverity(ID, clang::diag::Severity::Ignored, clang::SourceLocation());
            }
        }
        if(NeedsWerrorExclusion) {
            // FIXME: there's no API to suppress -Werror for single diagnostics.
            // In some cases with sub-groups, we may end up erroneously
            // downgrading diagnostics that were -Werror in the compile command.
            Diags.setDiagnosticGroupWarningAsError(Group, false);
        }
    }
}

ClangTidyChecker::ClangTidyChecker(std::unique_ptr<ClangTidyOptionsProvider> provider) :
    context(std::move(provider)) {}

clang::DiagnosticsEngine::Level
    ClangTidyChecker::adjustLevel(clang::DiagnosticsEngine::Level DiagLevel,
                                  const clang::Diagnostic& Info) {
    if(!checks.empty()) {
        std::string CheckName = context.getCheckName(Info.getID());
        bool IsClangTidyDiag = !CheckName.empty();
        if(IsClangTidyDiag) {
            // Check for suppression comment. Skip the check for diagnostics not
            // in the main file, because we don't want that function to query the
            // source buffer for preamble files. For the same reason, we ask
            // shouldSuppressDiagnostic to avoid I/O.
            // We let suppression comments take precedence over warning-as-error
            // to match clang-tidy's behaviour.
            bool IsInsideMainFile = Info.hasSourceManager() &&
                                    isInsideMainFile(Info.getLocation(), Info.getSourceManager());
            llvm::SmallVector<clang::tooling::Diagnostic, 1> TidySuppressedErrors;
            if(IsInsideMainFile && context.shouldSuppressDiagnostic(DiagLevel,
                                                                    Info,
                                                                    TidySuppressedErrors,
                                                                    /*AllowIO=*/false,
                                                                    /*EnableNolintBlocks=*/true)) {
                // FIXME: should we expose the suppression error (invalid use of
                // NOLINT comments)?
                return clang::DiagnosticsEngine::Ignored;
            }
            if(!context.getOptions().SystemHeaders.value_or(false) && Info.hasSourceManager() &&
               Info.getSourceManager().isInSystemMacro(Info.getLocation()))
                return clang::DiagnosticsEngine::Ignored;

            // Check for warning-as-error.
            if(DiagLevel == clang::DiagnosticsEngine::Warning && context.treatAsError(CheckName)) {
                return clang::DiagnosticsEngine::Error;
            }
        }
    }
    return DiagLevel;
}

void ClangTidyChecker::adjustDiag(Diagnostic& Diag) {
    std::string TidyDiag = context.getCheckName(Diag.id.value);
    if(!TidyDiag.empty()) {
        // TODO: using a global string saver.
        static llvm::BumpPtrAllocator Allocator;
        static llvm::StringSaver Saver(Allocator);
        Diag.id.name = Saver.save(TidyDiag);
        Diag.id.source = DiagnosticSource::ClangTidy;
        // clang-tidy bakes the name into diagnostic messages. Strip it out.
        // It would be much nicer to make clang-tidy not do this.
        auto CleanMessage = [&](std::string& Msg) {
            llvm::StringRef Rest(Msg);
            if(Rest.consume_back("]") && Rest.consume_back(Diag.id.name) && Rest.consume_back(" ["))
                Msg.resize(Rest.size());
        };
        CleanMessage(Diag.message);
        // todo: where is clice notes and fixes?
        // for(auto& Note: Diag.Notes)
        //     CleanMessage(Note.Message);
        // for(auto& Fix: Diag.Fixes)
        //     CleanMessage(Fix.Message);
    }
}

std::unique_ptr<ClangTidyChecker> configure(clang::CompilerInstance& instance,
                                            const TidyParams& params) {
    auto& input = instance.getFrontendOpts().Inputs[0];

    if(!input.isFile()) {
        return nullptr;
    }
    auto FileName = input.getFile();
    log::info("Tidy configure file: {}", FileName);

    tidy::ClangTidyOptions ClangTidyOpts = createTidyOptions();
    if(ClangTidyOpts.Checks) {
        log::info("Tidy configure checks: {}", *ClangTidyOpts.Checks);
    }

    {
        // If clang-tidy is configured to emit clang warnings, we should too.
        //
        // Such clang-tidy configuration consists of two parts:
        //   - ExtraArgs: ["-Wfoo"] causes clang to produce the warnings
        //   - Checks: "clang-diagnostic-foo" prevents clang-tidy filtering them out
        //
        // In clang-tidy, diagnostics are emitted if they pass both checks.
        // When groups contain subgroups, -Wparent includes the child, but
        // clang-diagnostic-parent does not.
        //
        // We *don't* want to change the compile command directly. This can have
        // too many unexpected effects: breaking the command, interactions with
        // -- and -Werror, etc. Besides, we've already parsed the command.
        // Instead we parse the -W<group> flags and handle them directly.
        //
        // Similarly, we don't want to use Checks to filter clang diagnostics after
        // they are generated, as this spreads clang-tidy emulation everywhere.
        // Instead, we just use these to filter which extra diagnostics we enable.
        auto& Diags = instance.getDiagnostics();
        TidyDiagnosticGroups TidyGroups(ClangTidyOpts.Checks ? *ClangTidyOpts.Checks
                                                             : llvm::StringRef());
        if(ClangTidyOpts.ExtraArgsBefore)
            applyWarningOptions(*ClangTidyOpts.ExtraArgsBefore, TidyGroups, Diags);
        if(ClangTidyOpts.ExtraArgs)
            applyWarningOptions(*ClangTidyOpts.ExtraArgs, TidyGroups, Diags);
    }

    /// No need to run clang-tidy or IncludeFixerif we are not going to surface
    /// diagnostics.
    const static auto* AllCTFactories = [] {
        auto* CTFactories = new tidy::ClangTidyCheckFactories;
        for(const auto& E: tidy::ClangTidyModuleRegistry::entries())
            E.instantiate()->addCheckFactories(*CTFactories);
        return CTFactories;
    }();
    tidy::ClangTidyCheckFactories FastFactories = filterFastTidyChecks(*AllCTFactories);
    std::unique_ptr<ClangTidyChecker> checker = std::make_unique<ClangTidyChecker>(
        std::make_unique<tidy::DefaultOptionsProvider>(tidy::ClangTidyGlobalOptions(),
                                                       ClangTidyOpts));

    checker->context.setDiagnosticsEngine(&instance.getDiagnostics());
    checker->context.setASTContext(&instance.getASTContext());
    // tood: is it always FileName to check?
    checker->context.setCurrentFile(FileName);
    checker->context.setSelfContainedDiags(true);
    checker->checks = FastFactories.createChecksForLanguage(&checker->context);
    log::info("Tidy configure checks: {}", checker->checks.size());
    clang::Preprocessor* PP = &instance.getPreprocessor();
    for(const auto& Check: checker->checks) {
        Check->registerPPCallbacks(instance.getSourceManager(), PP, PP);
        Check->registerMatchers(&checker->CTFinder);
    }

    return checker;
}

#if 0

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

#endif

}  // namespace clice::tidy
