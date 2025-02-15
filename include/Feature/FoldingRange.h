#include "Basic/Document.h"
#include "Basic/SourceCode.h"
#include "Basic/SourceConverter.h"
#include "Index/Shared.h"
#include "Support/JSON.h"

namespace clice {

class ASTInfo;

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

namespace feature::folding_range {

json::Value capability(json::Value clientCapabilities);

struct FoldingRange {
    LocalSourceRange range;
    proto::FoldingRangeKind kind;
    /// We don't record the coallaesced text for a range, because it's rarely useful.
};

using Result = std::vector<FoldingRange>;

/// Generate folding range for all files.
index::Shared<Result> foldingRange(ASTInfo& info, const SourceConverter& converter);

/// Return folding range in main file.
Result foldingRange(FoldingRangeParams& params, ASTInfo& info, const SourceConverter& converter);

/// Convert folding range to LSP format.
proto::FoldingRangeResult toLspResult(llvm::ArrayRef<FoldingRange> ranges, llvm::StringRef content,
                                      const SourceConverter& SC);

}  // namespace feature::folding_range

}  // namespace clice
