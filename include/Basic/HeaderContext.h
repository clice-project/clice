#include "llvm/ADT/StringRef.h"

namespace clice {

/// Represents a source location in a file, it is corresponding
/// to the `clang::SourceLocation` but decoded.
struct SourceLocation {
    /// The line number (1-based).
    uint32_t line;

    /// The column number (1-based).
    uint32_t column;

    /// The file name.
    std::string filename;
};

/// Describes the context of a header file to uniquely identify its AST.
/// A header file may generate different ASTs depending on the inclusion context.
/// Even within the same source file, the AST may vary at different locations due
/// to preprocessor directives, macro definitions, or other compilation settings.
struct HeaderContext {
    /// The compilation command used to generate the AST for the source file.
    std::string command;

    /// The inclusion chain of the header file.
    /// - The first element represents the header file itself.
    /// - The last element represents the top-level header file included in
    ///   the source file that leads to this header.
    /// This chain helps reconstruct the inclusion context for the header file.
    std::vector<SourceLocation> includeChain;
};

}  // namespace clice
