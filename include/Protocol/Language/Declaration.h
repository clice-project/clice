#pragma once

#include "../Basic.h"

namespace clice::protocol {

/// Client Capability:
/// - property name (optional): `textDocument.declaration`
/// - property type: `DeclarationClientCapabilities` defined as follows:
struct DeclarationClientCapabilities {
    /// Whether declaration supports dynamic registration. If this is set to
    ///`true` the client supports the new `DeclarationRegistrationOptions`
    /// return value for the corresponding server capability as well.
    bool dynamicRegistration = false;

    /// The client supports additional metadata in the form of declaration links.
    bool linkSupport = false;
};

/// Request:
/// - method: 'textDocument/declaration'
/// - params: `DeclarationParams` defined follows:
using DeclarationParams = Combine<TextDocumentPositionParams
                                  // WorkDoneProgressParams,
                                  // PartialResultParams,
                                  >;

/// Response:
/// result: Location
using DeclarationResult = Location;

}  // namespace clice::protocol
