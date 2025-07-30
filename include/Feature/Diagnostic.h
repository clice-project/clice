#pragma once

#include "Compiler/Diagnostic.h"
#include "Protocol/Feature/Diagnostic.h"
#include "Support/JSON.h"

namespace clice {

struct CompilationUnit;

}

namespace clice::feature {

/// FIXME: This is not correct way, we don't want to couple
/// `Feature with Protocol`?
json::Value diagnostics(CompilationUnit& unit);

}  // namespace clice::feature
