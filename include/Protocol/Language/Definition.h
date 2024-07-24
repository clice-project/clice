#include "../Basic.h"

namespace clice::protocol {

/// Client Capability:
/// - property name (optional): `textDocument.definition`
/// - property type: `DefinitionClientCapabilities` defined as follows:
struct DefinitionClientCapabilities {

    /// Whether definition supports dynamic registration.
    bool dynamicRegistration = false;

    /// The client supports additional metadata in the form of definition links.
    bool linkSupport = false;
};

/// Request:
/// - method: 'textDocument/definition'
/// - params: `DefinitionParams` defined follows:
using DefinitionParams = Combine<TextDocumentPositionParams
                                 // WorkDoneProgressParams,
                                 // PartialResultParams,
                                 >;

/// Response:
/// result: Location
using DefinitionResult = Location;

}  // namespace clice::protocol
