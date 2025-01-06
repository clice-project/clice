#pragma once

#include "Basic/Document.h"
#include "Compiler/Compiler.h"

namespace clice {

namespace proto {

// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#inlayHintParams
struct InlayHintParams {
    /// The text document.
    TextDocumentIdentifier textDocument;

    /// The visible document range for which inlay hints should be computed.
    Range range;
};

// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#inlayHintLabelPart
struct InlayHintLablePart {
    /// The label of the inlay hint.
    string value;

    /// The tooltip text when you hover over this label part.  Depending on
    /// the client capability `inlayHint.resolveSupport` clients might resolve
    /// this property late using the resolve request.
    MarkupContent tooltip;

    /// An optional source code location that represents this label part.
    Location Location;

    /// TODO:
    // Command command;
};

// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#inlayHintKind
struct InlayHintKind : refl::Enum<InlayHintKind> {
    enum Kind : uint8_t {
        Invalid = 0,
        Type = 1,
        Parameter = 2,
    };

    using Enum::Enum;

    constexpr static auto InvalidEnum = Invalid;
};

// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#inlayHint
struct InlayHint {
    /// The position of this hint.
    Position position;

    /// The label of this hint.
    std::vector<InlayHintLablePart> lable;

    /// The kind of this hint.
    InlayHintKind kind;

    /// TODO:
    /// Optional text edits that are performed when accepting this inlay hint.
    // std::vector<TextEdit> textEdits;

    /// Render padding before the hint.
    bool paddingLeft = false;

    /// Render padding before the hint.
    bool paddingRight = false;

    /// TODO:
    // LspAny data;
};

using InlayHintsResult = std::vector<InlayHint>;

}  // namespace proto

namespace config {

/// Configuration options for inlay hints, from table `inlay-hint` in `clice.toml`.
struct InlayHintConfig {
    uint32_t maxLength = 20;

    uint32_t maxArrayElements = 3;

    bool implicitCast = true;
};

}  // namespace config

namespace feature {

json::Value inlayHintCapability(json::Value InlayHintClientCapabilities);

/// Compute inlay hints for a document in given range and config.
proto::InlayHintsResult inlayHints(proto::InlayHintParams param, ASTInfo& ast,
                                   const config::InlayHintConfig& config);

}  // namespace feature

}  // namespace clice
