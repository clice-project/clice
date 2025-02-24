#include "Basic/SourceCode.h"
#include "Index/Shared.h"
#include "Support/Enum.h"

namespace clice {

class ASTInfo;

namespace feature {

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

/// We don't record the coalesced text for a range, because it's rarely useful.
struct FoldingRange {
    FoldingRangeKind kind;
    LocalSourceRange range;
};

/// Generate folding range for interested file only.
std::vector<FoldingRange> foldingRange(ASTInfo& AST);

/// Generate folding range for all files.
index::Shared<std::vector<FoldingRange>> indexFoldingRange(ASTInfo& AST);

}  // namespace feature

}  // namespace clice
