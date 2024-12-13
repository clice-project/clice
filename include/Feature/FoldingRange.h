#include "Basic/Document.h"
#include "Compiler/Compiler.h"

namespace clice {

struct FoldingRangeParams {};

namespace proto {

// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#foldingRangeClientCapabilities

/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#foldingRangeClientCapabilities
struct FoldingRangeClientCapabilities {};

/// TODO:
/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#foldingRangeParams
/// ```
/// export interface FoldingRangeParams extends WorkDoneProgressParams,
/// PartialResultParams {
/// ...
/// ```

struct FoldingRangeParams {

    /// The text document.
    TextDocumentIdentifier textDocument;
};

struct FoldingRangeKind : refl::Enum<FoldingRangeKind> {
    enum Kind : uint8_t {
        Invalid = 0,
        Comment,
        Imports,
        Region,
    };

    using Enum::Enum;

    constexpr static auto InvalidEnum = Invalid;
};

/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#foldingRange
struct FoldingRange {
    /// The zero-based start line of the range to fold. The folded area starts after the line's last
    /// character.
    uinteger startLine;

    /// The zero-based end line of the range to fold. The folded area ends with the line's last
    /// character.
    uinteger endLine;

    /// The zero-based character offset from where the folded range starts.
    uinteger startCharacter;

    /// The zero-based character offset before the folded range ends.
    uinteger endCharacter;

    /// Describes the kind of the folding range.
    FoldingRangeKind kind;

    // The text that the client should show when the specified range is collapsed.
    string collapsedText;
};

using FoldingRangeResult = std::vector<FoldingRange>;

}  // namespace proto

namespace feature {

/// TODO:
/// use `proto::FoldingRangeClientCapabilities` instead of `json::Value`to make a proper overload.
// json::Value capability(json::Value FoldingRangeClientCapabilities);

/// Return folding range in given file.
proto::FoldingRangeResult foldingRange(FoldingRangeParams& params, ASTInfo& ast);

}  // namespace feature

}  // namespace clice
