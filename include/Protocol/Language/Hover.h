#include "../Basic.h"

namespace clice::protocol {

/// Client Capability:
/// - property name (optional): `textDocument/hover`
/// - property type: `HoverClientCapabilities` defined as follows:
struct HoverClientCapabilities {

    /// Whether hover supports dynamic registration.
    bool dynamicRegistration = false;

    /// Client supports the follow content formats for the content property. The order describes the preferred
    /// format of the client.
    std::vector<MarkupKind> contentFormat;
};

/// Request:
/// - method: 'textDocument/hover'
/// - params: `HoverParams` defined follows:
using HoverParams = Combine<TextDocumentPositionParams
                            // WorkDoneProgressParams,
                            >;

/// The result of a hover request.
struct Hover {
    /// The hover's content
    MarkupContent contents;

    /// An optional range is a range inside a text document that is used to visualize a hover, e.g. by
    /// changing the background color.
    Range range;
};

/// Response:
/// result: Hover
using HoverResult = Hover;

}  // namespace clice::protocol
