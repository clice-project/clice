#pragma once

#include "clang-tidy/ClangTidyModuleRegistry.h"
#include "clang-tidy/ClangTidyOptions.h"
#include "clang-tidy/ClangTidyCheck.h"

#include "Compiler/Diagnostic.h"
#include "Compiler/Tidy.h"

namespace clice::tidy {

using namespace clang::tidy;

class ClangTidyChecker {

public:
    ClangTidyContext context;
    /// Create instances of checks that are enabled for the current Language.
    std::vector<std::unique_ptr<ClangTidyCheck>> checks;
    clang::ast_matchers::MatchFinder CTFinder;

    ClangTidyChecker(std::unique_ptr<ClangTidyOptionsProvider> provider);

    clang::DiagnosticsEngine::Level adjustLevel(clang::DiagnosticsEngine::Level DiagLevel,
                                                const clang::Diagnostic& Info);

    void adjustDiag(Diagnostic& Diag);
};

}  // namespace clice::tidy

namespace clice {

inline bool isInsideMainFile(clang::SourceLocation Loc, const clang::SourceManager& SM) {
    if(!Loc.isValid())
        return false;
    clang::FileID FID = SM.getFileID(SM.getExpansionLoc(Loc));
    return FID == SM.getMainFileID() || FID == SM.getPreambleFileID();
}

}  // namespace clice
