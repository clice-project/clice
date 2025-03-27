#pragma once

#include "AST/SymbolID.h"
#include "AST/SourceCode.h"
#include "Index/Shared.h"
#include "Support/JSON.h"

namespace clice::feature {

struct InlayHintKind : refl::Enum<InlayHintKind> {
    enum class Kind {};
};

struct InlayHint {
    /// The position offset of the inlay hint in the source code.
    uint32_t offset;

    /// The kind/category of the inlay hint.
    InlayHintKind kind;

    /// The label parts of the inlay hint.
    /// Each SymbolID consists of two parts: the symbol name and its USR hash.
    /// For symbols without a USR (e.g., built-in types or function parameters),
    /// the symbol hash will be empty.
    /// Otherwise, the symbol hash is non-empty and can be used for "go-to-definition".
    std::vector<index::SymbolID> parts;
};

using InlayHints = std::vector<InlayHint>;

InlayHints inlayHints(ASTInfo& AST, LocalSourceRange target);

index::Shared<InlayHints> indexInlayHints(ASTInfo& AST);

}  // namespace clice::feature
