#pragma once

#include "Test.h"
#include "Annotation.h"
#include "Compiler/Command.h"
#include "Compiler/Compilation.h"

namespace clice::testing {

/// A helper class for convenient ast building in testing.
struct Tester {
    /// Keep the compilation params.
    CompilationParams params;

    /// Keep the compilation database.
    CompilationDatabase database;

    /// Keep the compilation result.
    std::optional<CompilationUnit> unit;

    /// All sources file in the compilation.
    AnnotatedSources sources;
};

}  // namespace clice::testing
