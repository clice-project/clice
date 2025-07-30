#pragma once

#include "../Basic.h"

namespace clice::proto {

struct PublishDiagnosticsClientCapabilities {};

struct DiagnosticClientCapabilities {};

enum class DiagnosticSeverity : std::uint8_t {
    /// Reports an error.
    Error = 1,

    /// Reports a warning.
    Warning = 2,

    /// Reports an information.
    Information = 3,

    /// Reports a hint.
    Hint = 4,
};

enum class DiagnosticTag : std::uint8_t {
    /// Unused or unnecessary code. Clients are allowed to render diagnostics
    /// with this tag faded out instead of having an error squiggle.
    Unnecessary = 1,

    /// Deprecated or obsolete code. Clients are allowed to rendered
    /// diagnostics with this tag strike through.
    Deprecated = 2,
};

struct CodeDescription {
    /// An URI to open with more information about the diagnostic error.
    URI uri;
};

/// Represents a related message and source code location for a diagnostic.
/// This should be used to point to code locations that cause or are related to
/// a diagnostics, e.g when duplicating a symbol in a scope.
struct DiagnosticRelatedInformation {
    /// The location of this related diagnostic information.
    Location location;

    /// The message of this related diagnostic information.
    string message;
};

struct Diagnostic {
    /// The range at which the message applies.
    Range range;

    /// The diagnostic's severity. To avoid interpretation mismatches when a
    /// server is used with different clients it is highly recommended that
    /// servers always provide a severity value. If omitted, it’s recommended
    /// for the client to interpret it as an Error severity.
    DiagnosticSeverity severity;

    /// The diagnostic's code, which might appear in the user interface.
    string code;

    /// An optional property to describe the error code.
    optional<CodeDescription> codeDescription;

    /// A human-readable string describing the source of this
    /// diagnostic, e.g. 'typescript' or 'super lint'.
    string source;

    /// The diagnostic's message.
    string message;

    /// Additional metadata about the diagnostic.
    array<DiagnosticTag> tags;

    /// An array of related diagnostic information, e.g. when symbol-names within
    /// a scope collide all definitions can be marked via this property.
    array<DiagnosticRelatedInformation> relatedInformation;
};

}  // namespace clice::proto
