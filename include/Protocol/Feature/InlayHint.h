#pragma once

#include "../Basic.h"

namespace clice::proto {

struct InlayHintClientCapabilities {
    /// Indicates which properties a client can resolve lazily on an inlay hint.
    struct {
        /// The properties that a client can resolve lazily.
        array<string> properties;
    } resolveSupport;
};

struct InlayHintOptions {
    /// The server provides support to resolve additional
    /// information for an inlay hint item.
    bool resolveProvider;
};

struct InlayHintParams {
    /// The text document.
    TextDocumentIdentifier textDocument;

    /// The visible document range for which inlay hints should be computed.
    Range range;
};

enum class InlayHintKind {
    /// An inlay hint that for a type annotation.
    Type = 1,

    /// An inlay hint that is for a parameter.
    Parameter = 2,
};

struct InlayHintLabelPart {
    /// The value of this label part.
    string value;

    /// An optional source code location that represents this
    /// label part.
    ///
    /// The editor will use this location for the hover and for code navigation
    /// features: This part will become a clickable link that resolves to the
    /// definition of the symbol at the given location (not necessarily the
    /// location itself), it shows the hover that shows at the given location,
    /// and it shows a context menu with further code navigation commands.
    ///
    /// Depending on the client capability `inlayHint.resolveSupport` clients
    /// might resolve this property late using the resolve request.
    /// FIXME: Location location;
};

struct InlayHint {
    /// The position of this hint.
    ///
    /// If multiple hints have the same position, they will be shown in the order
    /// they appear in the response.
    Position position;

    /// The label of this hint. A human readable string or an array of
    /// InlayHintLabelPart label parts.
    ///
    /// *Note* that neither the string nor the label part can be empty.
    /// TODO: Use label
    array<InlayHintLabelPart> label;

    /// The kind of this hint. Can be omitted in which case the client
    ///  should fall back to a reasonable default.
    InlayHintKind kind;
};

}  // namespace clice::proto
