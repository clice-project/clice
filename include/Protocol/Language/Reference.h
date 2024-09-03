#pragma once

#include "../Basic.h"

namespace clice::protocol {

/// Client Capability:
/// - property name (optional): `textDocument.references`
/// - property type: `ReferenceClientCapabilities` defined as follows:
struct ReferenceClientCapabilities {

    /// Whether references supports dynamic registration.
    bool dynamicRegistration = false;
};

struct ReferenceContext {
    /// Include the declaration of the current symbol.
    bool includeDeclaration = false;
};

struct ReferenceParamsBody {
    ReferenceContext context;
};

/// Request:
/// - method: 'textDocument/references'
/// - params: `ReferenceParams` defined follows:
using ReferenceParams = Combine<TextDocumentPositionParams,
                                // WorkDoneProgressParams,
                                // PartialResultParams,
                                ReferenceParamsBody>;

/// Response:
/// result: Location[]
using ReferenceResult = std::vector<Location>;

}  // namespace clice::protocol
