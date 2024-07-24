#include "../Basic.h"

namespace clice::protocol {

/// Client Capability:
/// - property name (optional): `textDocument.documentHighlight`
/// - property type: `DocumentHighlightClientCapabilities` defined as follows:
struct DocumentHighlightClientCapabilities {

    /// Whether documentHighlight supports dynamic registration.
    bool dynamicRegistration = false;
};

/// Request:
/// - method: 'textDocument/documentHighlight'
/// - params: `DocumentHighlightParams` defined follows:
using DocumentHighlightParams = Combine<TextDocumentPositionParams
                                        // WorkDoneProgressParams,
                                        // PartialResultParams,
                                        >;
/// A document highlight kind.
enum class DocumentHighlightKind {
    /// A textual occurrence.
    Text = 1,
    /// Read-access of a symbol, like reading a variable.
    Read = 2,
    /// Write-access of a symbol, like writing to a variable.
    Write = 3,
};

/// A document highlight is a range inside a text document which deserves
/// special attention. Usually a document highlight is visualized by changing
/// the background color of its range.
struct DocumentHighlight {
    /// The range this highlight applies to.
    Range range;

    /// The highlight kind, default is DocumentHighlightKind.Text.
    DocumentHighlightKind kind = DocumentHighlightKind::Text;
};

/// Response:
/// result: DocumentHighlight[]
using DocumentHighlightResult = std::vector<DocumentHighlight>;

}  // namespace clice::protocol
