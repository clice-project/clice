#include "DocumentSymbol.h"

namespace clice::protocol {

/*=========================================================================/
/                                                                          /
/============================= CallHierarchy ==============================/
/                                                                          /
/=========================================================================*/

struct CallHierarchyItem {
    /// The name of this item.
    String name;

    /// The kind of this item.
    SymbolKind kind;

    /// Tags for this item.
    std::vector<SymbolTag> tags;

    /// More detail for this item, e.g. the signature of a function.
    std::string detail;

    /// The resource identifier of this item.
    URI uri;

    /// The range enclosing this symbol not including leading/trailing whitespace but everything else, e.g.
    /// comments and code.
    Range range;

    /// The range that should be selected and revealed when this symbol is being picked, e.g. the name of a
    /// function.
    Range selectionRange;

    /// A data entry field that is preserved between a call hierarchy prepare and incoming calls or outgoing
    /// calls requests. std::any data;
    /// data?: LSPAny;
};

/// Client Capability:
/// - property name (optional): `textDocument.callHierarchy`
/// - property type: `CallHierarchyClientCapabilities` defined as follows:
struct CallHierarchyClientCapabilities {

    /// Whether callHierarchy supports dynamic registration.
    bool dynamicRegistration = false;
};

/// Request:
/// - method: 'textDocument/prepareCallHierarchy'
/// - params: `PrepareCallHierarchyParams` defined follows:
using CallHierarchyPrepareParams = Combine<TextDocumentPositionParams
                                           // WorkDoneProgressParams,
                                           >;

/// Response:
/// - result: `CallHierarchyItem[]`
using CallHierarchyPrepareResult = std::vector<CallHierarchyItem>;

/*=========================================================================/
/                                                                          /
/====================== Call Hierarchy Incoming Calls =====================/
/                                                                          /
/=========================================================================*/

struct CallHierarchyIncomingCallsParamsBody {
    /// The item for which incoming calls are to be computed.
    CallHierarchyItem item;
};

/// Request:
/// - method: 'textDocument/callHierarchy/incomingCalls'
/// - params: `CallHierarchyIncomingCallsParams` defined follows:
using CallHierarchyIncomingCallsParams = Combine<TextDocumentPositionParams,
                                                 // WorkDoneProgressParams,
                                                 // PartialResultParams,
                                                 CallHierarchyIncomingCallsParamsBody>;

struct CallHierarchyIncomingCall {
    /// The item that was called.
    CallHierarchyItem from;
    /// The range at which at which the calls were made.
    Range fromRanges;
};

/// Response:
/// - result: `CallHierarchyIncomingCall[]`
using CallHierarchyIncomingCallsResult = std::vector<CallHierarchyIncomingCall>;

}  // namespace clice::protocol
