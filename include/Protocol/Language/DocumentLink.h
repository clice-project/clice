#include "../Basic.h"

namespace clice::protocol {

/// Client Capability:
/// - property name (optional): `textDocument.documentLink`
/// - property type: `DocumentLinkClientCapabilities` defined as follows:
struct DocumentLinkClientCapabilities {

    /// Whether documentLink supports dynamic registration.
    bool dynamicRegistration = false;

    /// Whether the client support the `tooltip` property on `DocumentLink`.
    bool tooltipSupport = false;
};

struct DocumentLinkParamsBody {
    /// The document to provide document links for.
    TextDocumentIdentifier textDocument;
};

/// Request:
/// - method: 'textDocument/documentLink'
/// - params: `DocumentLinkParams` defined follows:
using DocumentLinkParams = Combine<
    // WorkDoneProgressParams,
    // PartialResultParams,
    DocumentLinkParamsBody>;

/// A document link is a range in a text document that links to an internal or
/// external resource, like another text document or a web site.
struct DocumentLink {
    /// The range this link applies to.
    Range range;

    /// The uri this link points to. If missing a resolve request is sent later.
    std::string target;

    /// The tooltip text when you hover over this link.
    std::string tooltip;

    // data?: LSPAny;
};

/// Response:
/// - result: `DocumentLink[]`
using DocumentLinkResult = std::vector<DocumentLink>;

}  // namespace clice::protocol

