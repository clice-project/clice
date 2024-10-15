#pragma once

#include <Compiler/Clang.h>

namespace clice {

// FIXME: currently, we do not use preamble.
// NOTE: if preamble is used for a TU, the header tokens can not find in TokenBuffer.

/// Represents the preamble of a translation unit.
/// We build preamble for the translation after first edit.
/// The preamble is used to speed up the reparse of the translation unit.
struct Preamble {};

}  // namespace clice
