#pragma once

#include "DocumentSymbol.h"

namespace clice::protocol {

struct TypeHierarchyItem {

    /// The name of this item.
    String name;

    /// The kind of this item.
    SymbolKind kind;

    /// Tags for this item.
    std::vector<SymbolTag> tags;

    /// More detail for this item, e.g. the signature of a function.
    std::string detail;

    /// The resource identifier of this item.
    DocumentUri uri;

    /// The range enclosing this symbol not including leading/trailing whitespace but everything else, e.g.
    /// comments and code.
    Range range;

    /// The range that should be selected and revealed when this symbol is being picked, e.g. the name of a
    /// function.
    Range selectionRange;

    /// A data entry field that is preserved between a call hierarchy prepare and incoming calls or outgoing
    /// calls requests.
    /// data?: LSPAny;
};

/// Client Capability:
/// - property name (optional): `textDocument/typeHierarchy`
/// - property type: `TypeHierarchyClientCapabilities` defined as follows:
struct TypeHierarchyClientCapabilities {
    /// Whether typeHierarchy supports dynamic registration.
    bool dynamicRegistration = false;
};

/// Request:
/// - method: 'textDocument/prepareTypeHierarchy'
/// - params: `TypeHierarchyParams` defined follows:
using TypeHierarchyParams = Combine<TextDocumentPositionParams
                                    // WorkDoneProgressParams,
                                    >;

/// Response:
/// - result: `TypeHierarchyItem[]`
using TypeHierarchyResult = std::vector<TypeHierarchyItem>;

/*=========================================================================/
/                                                                          /
/======================== Type Hierarchy Supertypes =======================/
/                                                                          /
/=========================================================================*/

// TODO:
}  // namespace clice::protocol
