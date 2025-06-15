#pragma once

#include <cstdint>
#include <string>
#include "AST/SourceCode.h"

namespace clang {
class DiagnosticConsumer;
}

namespace clice {

enum class DiagnosticLevel : std::uint8_t {
    Ignored,
    Note,
    Remark,
    Warning,
    Error,
    Fatal,
    Invalid,
};

struct Diagnostic {
    /// The diagnostic id.
    std::uint32_t id;

    /// The level of this diagnostic.
    DiagnosticLevel level;

    /// The source range of this diagnostic(may be invalid, if this diagnostic
    /// is from command line. e.g. unknown command line argument).
    clang::SourceRange range;

    /// The error message of this diagnostic.
    std::string message;

    /// TODO: Collect fix it of diagnostics.

    static llvm::StringRef diagnostic_code(std::uint32_t id);

    static clang::DiagnosticConsumer* create(std::vector<Diagnostic>& diagnostics);
};

}  // namespace clice
