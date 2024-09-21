#pragma once

#include "Directive.h"

namespace clice {

// FIXME: currently, we do not use preamble.
// NOTE: if preamble is used for a TU, the header tokens can not find in TokenBuffer.

/// Represents the preamble of a translation unit.
/// We build preamble for the translation after first edit.
/// The preamble is used to speed up the reparse of the translation unit.
struct Preamble {
    clang::PrecompiledPreamble data;

    static std::unique_ptr<Preamble> build(llvm::StringRef filename,
                                           llvm::StringRef content,
                                           std::vector<const char*>& args);
};

}  // namespace clice
