#pragma once

#include "../Basic.h"

namespace clice::proto {

enum class SymbolKind : std::uint8_t {
    File = 1,
    Module = 2,
    Namespace = 3,
    Package = 4,
    Class = 5,
    Method = 6,
    Property = 7,
    Field = 8,
    Constructor = 9,
    Enum = 10,
    Interface = 11,
    Function = 12,
    Variable = 13,
    Constant = 14,
    String = 15,
    Number = 16,
    Boolean = 17,
    Array = 18,
    Object = 19,
    Key = 20,
    Null = 21,
    EnumMember = 22,
    Struct = 23,
    Event = 24,
    Operator = 25,
    TypeParameter = 26,
};

enum class SymbolTag {
    /// Render a symbol as obsolete, usually using a strike-out.
    Deprecated = 1,
};

struct DocumentSymbolClientCapabilities {
    /// Specific capabilities for the `SymbolKind` in the
    /// `textDocument/documentSymbol` request.
    struct {
        /// The symbol kind values the client supports. When this
        /// property exists the client also guarantees that it will
        /// handle values outside its set gracefully and falls back
        /// to a default value when unknown.
        //
        /// If this property is not present the client only supports
        /// the symbol kinds from `File` to `Array` as defined in
        /// the initial version of the protocol.
        array<SymbolKind> valueSet;
    } symbolKind;

    /// The client supports hierarchical document symbols.
    bool hierarchicalDocumentSymbolSupport;

    /// The client supports tags on `SymbolInformation`. Tags are supported on
    /// `DocumentSymbol` if `hierarchicalDocumentSymbolSupport` is set to true.
    /// Clients supporting tags have to handle unknown tags gracefully.
    struct {
        /// The tags supported by the client.
        array<SymbolTag> valueSet;
    } tagSupport;

    /// The client supports an additional label presented in the UI when
    /// registering a document symbol provider.
    bool labelSupport;
};

struct DocumentSymbolOptions {};

struct DocumentSymbolParams {
    /// The text document.
    TextDocumentIdentifier textDocument;
};

/// Represents programming constructs like variables, classes, interfaces etc.
/// that appear in a document. Document symbols can be hierarchical and they
/// have two ranges: one that encloses its definition and one that points to its
/// most interesting range, e.g. the range of an identifier.
struct DocumentSymbol {
    /// The name of this symbol. Will be displayed in the user interface and
    /// therefore must not be an empty string or a string only consisting of
    /// white spaces.
    string name;

    /// More detail for this symbol, e.g the signature of a function.
    string detail;

    /// The kind of this symbol.
    SymbolKind kind;

    /// Tags for this document symbol.
    array<SymbolTag> tags;

    /// The range enclosing this symbol not including leading/trailing whitespace
    /// but everything else like comments. This information is typically used to
    /// determine if the clients cursor is inside the symbol to reveal it  in the
    /// UI.
    Range range;

    /// The range that should be selected and revealed when this symbol is being
    /// picked, e.g. the name of a function. Must be contained by the `range`.
    Range selectionRange;

    /// Children of this symbol, e.g. properties of a class.
    array<DocumentSymbol> children;
};

}  // namespace clice::proto
