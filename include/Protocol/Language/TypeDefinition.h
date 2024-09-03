#pragma once

#include "../Basic.h"

namespace clice::protocol {

/// Client Capability:
/// - property name (optional): `textDocument/typeDefinition`
/// - property type: `TypeDefinitionClientCapabilities` defined as follows:
struct TypeDefinitionClientCapabilities {

    /// Whether typeDefinition supports dynamic registration.
    bool dynamicRegistration = false;

    /// The client supports additional metadata in the form of typeDefinition links.
    bool linkSupport = false;
};

/// Request:
/// - method: 'textDocument/typeDefinition'
/// - params: `TypeDefinitionParams` defined follows:
using TypeDefinitionParams = Combine<TextDocumentPositionParams
                                     // WorkDoneProgressParams,
                                     // PartialResultParams,
                                     >;

/// Response:
/// result: Location
using TypeDefinitionResult = Location;

}  // namespace clice::protocol
