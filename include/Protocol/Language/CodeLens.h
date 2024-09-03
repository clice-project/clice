#pragma once

#include "../Basic.h"

namespace clice::protocol {

/// Client Capability:
/// - property name (optional): `textDocument/codeLens`
/// - property type: `CodeLensClientCapabilities` defined as follows:
struct CodeLensClientCapabilities {
    /// Whether codeLens supports dynamic registration.
    bool dynamicRegistration = false;
};

struct CodeLensParamsBody {
    /// The document to request code lens for.
    TextDocumentIdentifier textDocument;
};

/// Request:
/// - method: 'textDocument/codeLens'
/// - params: `CodeLensParams` defined follows:
using CodeLensParams = Combine<
    // WorkDoneProgressParams,
    // PartialResultParams,
    CodeLensParamsBody>;

struct CodeLens {
    /// The range in which this code lens is valid. Should only span a single line.
    Range range;

    /// The command this code lens represents.
    /// TODO: Command command;

    /// data?: LSPAny;
};

}  // namespace clice::protocol
