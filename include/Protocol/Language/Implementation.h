#include "../Basic.h"

namespace clice::protocol {

/// Client Capability:
/// - property name (optional): `textDocument.implementation`
/// - property type: `ImplementationClientCapabilities` defined as follows:
struct ImplementationClientCapabilities {

    /// Whether implementation supports dynamic registration.
    bool dynamicRegistration = false;

    /// The client supports additional metadata in the form of implementation links.
    bool linkSupport = false;
};

/// Request:
/// - method: 'textDocument/implementation'
/// - params: `ImplementationParams` defined follows:
using ImplementationParams = Combine<TextDocumentPositionParams
                                     // WorkDoneProgressParams,
                                     // PartialResultParams,
                                     >;

/// Response:
/// result: Location
using ImplementationResult = Location;

}  // namespace clice::protocol
