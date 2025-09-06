#pragma once

#include "llvm/ADT/StringRef.h"

#include <memory>

namespace clang {
class CompilerInstance;
}

namespace clice::tidy {

bool isRegisteredTidyCheck(llvm::StringRef check);
std::optional<bool> isFastTidyCheck(llvm::StringRef check);

struct TidyParams {};

class ClangTidyChecker;

/// Run clang-tidy on the given file.
std::unique_ptr<ClangTidyChecker> configure(clang::CompilerInstance& instance,
                                            const TidyParams& params);

}  // namespace clice::tidy
