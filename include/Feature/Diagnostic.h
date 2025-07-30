#pragma once

#include "Compiler/Diagnostic.h"
#include "Server/Convert.h"
#include "Support/JSON.h"

namespace clice {

struct CompilationUnit;

}

namespace clice::feature {

/// FIXME: This is not correct way, we don't want to couple
/// `Feature with Protocol`? Return an array of LSP diagnostic.
json::Value diagnostics(PositionEncodingKind kind, PathMapping mapping, CompilationUnit& unit);

}  // namespace clice::feature
