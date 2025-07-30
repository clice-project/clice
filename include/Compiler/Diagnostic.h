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

enum class DiagnosticSource : std::uint8_t {
    Unknown,
    Clang,
    ClangTidy,
    Clice,
};

struct DiagnosticID {
    /// The diagnostic id value.
    std::uint32_t value;

    /// The level of this diagnostic.
    DiagnosticLevel level;

    /// The source of diagnostic.
    DiagnosticSource source;

    llvm::StringRef name;

    /// Get the diagnostic code.
    llvm::StringRef diagnostic_code();

    /// Get help diagnostic uri for the diagnostic.
    std::optional<std::string> diagnostic_document_uri();
};

struct Diagnostic {
    /// The diagnostic id.
    DiagnosticID id;

    /// The source range of this diagnostic(may be invalid, if this diagnostic
    /// is from command line. e.g. unknown command line argument).
    clang::SourceRange range;

    /// The error message of this diagnostic.
    std::string message;

    static clang::DiagnosticConsumer* create(std::shared_ptr<std::vector<Diagnostic>> diagnostics);
};

}  // namespace clice
