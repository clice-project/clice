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
struct InlayHintOption {
    // Max length of hint string, the extra part will be replaced with `...`
    uint16_t maxLength = 30;

    // How many elements to show in array/initializer-list.
    uint16_t maxArrayElements = 3;

    // Hint for `auto` declaration, structure binding, if/for statement with initializer.
    bool dedcucedType : 1 = true;

    // Hint for  function / lambda return type.
    bool returnType : 1 = true;

    // Hint after '}', including if/switch/while/for/namespace/class/function end.
    bool blockEnd : 1 = false;

    // Hint for function arguments.
    bool argumentName : 1 = true;

    /// TODO:
    /// Hint for implicit cast like `1 |as int|`.
    bool implicitCast : 1 = false;

    /// TODO:
    /// Hint for function return type in multiline chaind-call. e.g.
    ///     a()
    ///     .to_b() |ClassB|
    ///     .to_c() |ClassC|
    ///     .to_d() |ClassD|
    bool chainCall : 1 = false;
};

}  // namespace config

namespace feature {

json::Value inlayHintCapability(json::Value InlayHintClientCapabilities);

/// Compute inlay hints for a document in given range and config.
proto::InlayHintsResult inlayHints(proto::InlayHintParams param, ASTInfo& ast,
                                   const config::InlayHintOption& config);

}  // namespace feature

}  // namespace clice
