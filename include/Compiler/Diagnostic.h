#pragma once

#include "Clang.h"
#include "Basic/Document.h"

namespace clice {
namespace proto {

/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#diagnosticSeverity
struct DiagnosticSeverity : refl::Enum<DiagnosticSeverity> {
    enum Kind : uint8_t {
        Invalid = 0,
        Error = 1,
        Warning = 2,
        Information = 3,
        Hint = 4,
    };

    using Enum = refl::Enum<DiagnosticSeverity>;
    using Enum::Enum;
};

/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#diagnosticTag
struct DiagnosticTag : refl::Enum<DiagnosticTag> {
    enum Kind : uint8_t {
        Invalid = 0,
        Unnecessary = 1,
        Deprecated = 2,
    };

    using Enum = refl::Enum<DiagnosticTag>;
    using Enum::Enum;
};

/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#diagnosticRelatedInformation
struct DiagnosticRelatedInformation {
    /// The location of this related diagnostic information.
    Location location;

    /// The message of this related diagnostic information.
    std::string message;
};

/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#codeDescription
struct CodeDescription {
    /// An URI to open with more information about the diagnostic error.
    /// e.g. https://clang.llvm.org/docs/DiagnosticsReference.html
    URI href;
};

/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#diagnostic
struct Diagnostic {
    /// The range at which the message applies.
    Range range;

    /// The diagnostic's severity.
    uinteger severity;

    /// The diagnostic's code.
    std::string code = "";

    /// A human-readable string describing the source of this diagnostic, e.g.
    /// 'clang' (a compile error), 'clang-tidy' (from tidy), 'clice' (include error).
    std::string source;

    /// The diagnostic's message.
    std::string message;

    /// An array of related diagnostic information, e.g. when symbol-names within a scope collide
    /// all definitions can be marked via this property.
    std::vector<DiagnosticRelatedInformation> relatedInformation;

    /// Additional metadata about the diagnostic.
    uinteger tags;

    /// An URI to open with more information about the diagnostic error.
    CodeDescription codeDescription;

    /// TODO:
    /// LSPAny data;
};

}  // namespace proto
}  // namespace clice

namespace clang::tidy {

class ClangTidyContext;

}  // namespace clang::tidy

namespace clice {

/// Some options for diagnostics.
struct DiagOption {
    /// If true, clice will add a number of available fixes to the diagnostic's
    /// message. e.g.
    ///     "error: 'foo' is not defined; did you mean 'bar'? (2 fixes available)"
    bool ShowFixesCount = true;
};

/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#diagnosticSeverity
enum class DiagSeverity : uint8_t {
    // Ignore is a invalid diagnosticSeverity.
    Ignore = 0,

    Error = 1,
    Warning = 2,
    Info = 3,
    Hint = 4,
};

/// Same as `proto::DiagnosticRelatedInformation`, but represented in clang AST.
struct DiagRelatedInfo {
    clang::SourceLocation location;
    std::string message;
};

struct DiagnosticBase {

    std::string message;

    /// The SourceRange of the diagnostic.
    clang::SourceRange range;

    /// Since the `#include`, a flag is needed to distinguish diags comes from the MainFileID.
    bool isInMainFile;

    DiagSeverity severity;
};

struct Note : DiagnosticBase {};

struct Fix {
    /// Message for the fix-it.
    std::string message;

    // /// TextEdits from clang's fix-its. Must be non-empty.
    // llvm::SmallVector<TextEdit, 1> Edits;

    // /// Annotations for the Edits.
    // llvm::SmallVector<std::pair<ChangeAnnotationIdentifier, ChangeAnnotation>> Annotations;
};

/// Which source the diagnostic comes from.
enum class DiagSource : uint8_t {
    /// Comes from the compiler.
    Clang,

    /// Comes from clang-tidy.
    ClangTidy,

    /// Comes from Clice itself.
    Clice,
};

enum class DiagTag : uint8_t {
    Unnecessary = 1,
    Deprecated = 2,
};

struct Diagnostic : DiagnosticBase {
    DiagSource source;

    /// e.g. member of clang::diag, or clang-tidy assigned ID.
    uint32_t ID = 0;

    /// If ID is recognized, this will be the name of the diagnostic. By default it's empty.
    /// e.g. "misc-definitions-in-headers".
    llvm::StringRef name = "";

    /// If ID is recognized, comes from `clang::DiagnosticIDs::getCategoryNameFromID(category)`.
    llvm::StringRef category = "";

    /// Available fixes.
    llvm::SmallVector<Fix, 1> fixes;

    /// Related notes.
    llvm::SmallVector<Note, 1> notes;

    llvm::SmallVector<DiagTag, 2> tag;
};

/// TODO:
/// Find a proper way to convert clice::Diagnostic to proto::Diagnostic.
/// proto::Diagnostic toLspDiagnostic(Diagnostic& diag);

/// A diagnostic collector that collects diagnostics from the compiler.
class DiagnosticCollector : public clang::DiagnosticConsumer {
public:
    using Diagnostics = std::vector<Diagnostic>;

    void BeginSourceFile(const clang::LangOptions& Opts, const clang::Preprocessor* PP) override;

    void HandleDiagnostic(clang::DiagnosticsEngine::Level DiagLevel,
                          const clang::Diagnostic& Info) override;

    void EndSourceFile() override;

    /// Move out inner diags from the collector and populate `source`, `ID` and `name` for
    /// clang-tidy diagnostics.  If `ctx` is nullptr, return `diags` directly.
    Diagnostics takeWithTidyContext(const clang::tidy::ClangTidyContext* ctx);

    Diagnostics take() {
        return takeWithTidyContext(nullptr);
    };

private:
    const clang::SourceManager* originSrcMgr = nullptr;
    const clang::LangOptions* langOpts = nullptr;
    Diagnostics diags;
};

}  // namespace clice
