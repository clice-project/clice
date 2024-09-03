#pragma once

#include "../Basic.h"

namespace clice::protocol {

///  A symbol kind.
enum class SymbolKind : uint8_t {
    File = 1,
    Module,
    Namespace,
    Package,
    Class,
    Method,
    Property,
    Field,
    Constructor,
    Enum,
    Interface,
    Function,
    Variable,
    Constant,
    String,
    Number,
    Boolean,
    Array,
    Object,
    Key,
    Null,
    EnumMember,
    Struct,
    Event,
    Operator,
    TypeParameter
};

/// Symbol tags are extra annotations that tweak the rendering of a symbol.
enum class SymbolTag : uint8_t {
    /// Render a symbol as obsolete, usually using a strike-out.
    Deprecated = 1,
};

/// Represents programming constructs like variables, classes, interfaces etc.
/// that appear in a document. Document symbols can be hierarchical and they
/// have two ranges: one that encloses its definition and one that points to its
/// most interesting range, e.g. the range of an identifier.
struct DocumentSymbol {
    /// The name of this symbol.
    String name;

    /// More detail for this symbol, e.g the signature of a function.
    String detail;

    /// The kind of this symbol.
    SymbolKind kind;

    /// Tags for this symbol.
    std::vector<SymbolTag> tags;

    /// The range enclosing this symbol not including leading/trailing whitespace but everything else, e.g.
    /// comments and code.
    Range range;

    /// The range that should be selected and revealed when this symbol is being picked, e.g. the name of a
    /// function. Must be contained by the `range`.
    Range selectionRange;

    /// Children of this symbol, e.g. properties of a class.
    std::vector<DocumentSymbol> children;
};

}  // namespace clice::protocol
