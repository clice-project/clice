#pragma once

#include "AST/SourceCode.h"
#include "Index/Shared.h"
#include "Support/Enum.h"

namespace clice::feature {

struct FoldingRangeKind : refl::Enum<FoldingRangeKind> {
    enum Kind : uint8_t {
        Invalid = 0,
        Comment,
        Imports,
        Region,
        Namespace,
        Class,
        Enum,
        Struct,
        Union,
        LambdaCapture,
        FunctionParams,
        FunctionBody,
        FunctionCall,
        CompoundStmt,
        AccessSpecifier,
        ConditionDirective,
        Initializer,
    };

    using Enum::Enum;

    constexpr static auto InvalidEnum = Invalid;
};

/// We don't record the coalesced text for a range, because it's rarely useful.
struct FoldingRange {
    /// The range to fold.
    LocalSourceRange range;

    /// Describes the kind of the folding range.
    FoldingRangeKind kind;

    /// The text to display when the folding range is collapsed.
    std::string text;
};

/// Generate folding range for interested file only.
std::vector<FoldingRange> foldingRange(ASTInfo& AST);

/// Generate folding range for all files.
index::Shared<std::vector<FoldingRange>> indexFoldingRange(ASTInfo& AST);

}  // namespace clice::feature

