#pragma once

#include "../Basic.h"

namespace clice::proto {

struct DocumentLinkClientCapabilities {
    /// Whether the client supports the `tooltip` property on `DocumentLink`.
    bool tooltipSupport = false;
};

struct DocumentLinkOptions {
    /// Document links have a resolve provider as well.
    bool resolveProvider;
};

struct DocumentLinkParams {
    /// The document to provide document links for.
    TextDocumentIdentifier textDocument;
};

/// A document link is a range in a text document that links to an internal or
/// external resource, like another text document or a web site.
struct DocumentLink {
    /// The range this link applies to.
    Range range;

    /// The uri this link points to. If missing a resolve request is sent later.
    URI target;

    /// The tooltip text when you hover over this link.
    ///
    /// If a tooltip is provided, is will be displayed in a string that includes
    /// instructions on how to trigger the link, such as `{0} (ctrl + click)`.
    /// The specific instructions vary depending on OS, user settings, and
    /// localization.
    /// FIXME: string tooltip;
};

}  // namespace clice::proto
