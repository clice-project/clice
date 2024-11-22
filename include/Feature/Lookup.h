#include "Basic/Document.h"

namespace clice::proto {

using DeclarationParams = TextDocumentPositionParams;

using DeclarationResult = std::vector<Location>;

using DefinitionParams = TextDocumentPositionParams;

using DefinitionResult = std::vector<Location>;

using TypeDefinitionParams = TextDocumentPositionParams;

using TypeDefinitionResult = std::vector<Location>;

using ImplementationParams = TextDocumentPositionParams;

using ImplementationResult = std::vector<Location>;

using ReferenceParams = TextDocumentPositionParams;

using ReferenceResult = std::vector<Location>;

using CallHierarchyPrepareParams = TextDocumentPositionParams;

struct CallHierarchyItem {
    /// The name of this item.
    string name;

    /// FIXME: ...
};

using CallHierarchyPrepareResult = std::vector<CallHierarchyItem>;

struct CallHierarchyIncomingCallsParams {
    CallHierarchyItem item;
};

struct CallHierarchyIncomingCall {
    /// The item that makes the call.
    CallHierarchyItem from;

    /// The range at which at which the calls appears. This is relative to the caller
    /// denoted by `from`.
    std::vector<Range> fromRange;
};

using CallHierarchyIncomingCallsResult = std::vector<CallHierarchyIncomingCall>;

struct CallHierarchyOutgoingCallsParams {
    CallHierarchyItem item;
};

struct CallHierarchyOutgoingCall {
    /// The item that is called.
    CallHierarchyItem to;

    /// The range at which this item is called. This is relative to the caller
    /// denoted by `to`.
    std::vector<Range> toRange;
};

using CallHierarchyOutgoingCallsResult = std::vector<CallHierarchyOutgoingCall>;

struct TypeHierarchyItem {
    /// The name of the type.
    string name;

    /// FIXME: ...
};

using TypeHierarchyPrepareParams = TextDocumentPositionParams;

using TypeHierarchyPrepareResult = std::vector<TypeHierarchyItem>;

struct TypeHierarchySupertypesParams {
    TypeHierarchyItem item;
};

using TypeHierarchySupertypesResult = std::vector<TypeHierarchyItem>;

struct TypeHierarchySubtypesParams {
    TypeHierarchyItem item;
};

using TypeHierarchySubtypesResult = std::vector<TypeHierarchyItem>;

}  // namespace clice::proto
