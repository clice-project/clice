#pragma once

#include <Support/ADT.h>

namespace clice::dependencies {

void load(ArrayRef<StringRef> dirs);

/// Record the include graph of the translation unit.
struct IncludeGraph {};

/// Record the map between module name and file path.
struct ModuleMap {};

// TODO:

}  // namespace clice::dependency
