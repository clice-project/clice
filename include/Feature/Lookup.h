#include "Basic/Document.h"

namespace clice::proto {

struct WorkDoneProgressOptions {
    /// Report on work done progress.
    bool workDoneProgress = false;
};

using DeclarationOptions = WorkDoneProgressOptions;

using DeclarationParams = TextDocumentPositionParams;

using DeclarationResult = std::vector<Location>;

using DefinitionOptions = DeclarationParams;

using DefinitionParams = TextDocumentPositionParams;

using DefinitionResult = std::vector<Location>;

using TypeDefinitionOptions = WorkDoneProgressOptions;

using TypeDefinitionParams = TextDocumentPositionParams;

using TypeDefinitionResult = std::vector<Location>;

using ImplementationOptions = WorkDoneProgressOptions;

using ImplementationParams = TextDocumentPositionParams;

using ImplementationResult = std::vector<Location>;

using ReferenceOptions = WorkDoneProgressOptions;

using ReferenceParams = TextDocumentPositionParams;

using ReferenceResult = std::vector<Location>;

using CallHierarchyOptions = WorkDoneProgressOptions;

using CallHierarchyPrepareParams = TextDocumentPositionParams;

struct HierarchyItem {
    /// The name of this item.
    string name;

    /// The resource identifier of this item.
    string uri;

    /// The range enclosing this symbol not including leading/trailing whitespace
    /// but everything else, e.g. comments and code.
    Range range;

    /// The range that should be selected and revealed when this symbol is being
    /// picked, e.g. the name of a function. Must be contained by the
    /// [`range`](#CallHierarchyItem.range).
    Range selectionRange;

    /// A data entry field that is preserved between a call hierarchy prepare and
    /// incoming calls or outgoing calls requests.
    uint64_t data = 0;
};

struct HierarchyParams {
    /// The item for which to return the call hierarchy.
    HierarchyItem item;
};

using CallHierarchyItem = HierarchyItem;

using CallHierarchyPrepareResult = std::vector<CallHierarchyItem>;

using CallHierarchyIncomingCallsParams = HierarchyParams;

struct CallHierarchyIncomingCall {
    /// The item that makes the call.
    CallHierarchyItem from;

    /// The range at which at which the calls appears. This is relative to the caller
    /// denoted by `from`.
    std::vector<Range> fromRange;
};

using CallHierarchyIncomingCallsResult = std::vector<CallHierarchyIncomingCall>;

using CallHierarchyOutgoingCallsParams = HierarchyItem;

struct CallHierarchyOutgoingCall {
    /// The item that is called.
    CallHierarchyItem to;

    /// The range at which this item is called. This is relative to the caller
    /// denoted by `to`.
    std::vector<Range> toRange;
};

using CallHierarchyOutgoingCallsResult = std::vector<CallHierarchyOutgoingCall>;

using TypeHierarchyOptions = WorkDoneProgressOptions;

using TypeHierarchyItem = HierarchyItem;

using TypeHierarchyPrepareParams = TextDocumentPositionParams;

using TypeHierarchyPrepareResult = std::vector<TypeHierarchyItem>;

using TypeHierarchySupertypesParams = HierarchyParams;

using TypeHierarchySupertypesResult = std::vector<TypeHierarchyItem>;

using TypeHierarchySubtypesParams = HierarchyParams;

using TypeHierarchySubtypesResult = std::vector<TypeHierarchyItem>;

}  // namespace clice::proto
