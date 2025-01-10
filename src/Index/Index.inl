#ifndef MAKE_CLANGD_HAPPY

#include "Index/SymbolIndex.h"

using namespace clice;
using namespace clice::index;

template <typename T>
struct Array {};

using String = Array<char>;

#endif

struct ValueRef {
    uint32_t offset = std::numeric_limits<uint32_t>::max();

    bool valid() const {
        return offset != std::numeric_limits<uint32_t>::max();
    }

    operator uint32_t () const {
        return offset;
    }
};

struct Relation {
    RelationKind kind;

    /// The `data` array contains two fields whose meanings depend on the `kind`.
    /// Each `RelationKind` specifies the interpretation of these fields as follows:
    ///
    /// - `Definition` and `Declaration`:
    ///   - `data[0]`: The range of the name token.
    ///   - `data[1]`: The range of the whole symbol.
    ///
    /// - `Reference` and `WeakReference`:
    ///   - `data[0]`: The range of the reference.
    ///   - `data[1]`: Empty (unused).
    ///
    /// - `Interface`, `Implementation`, `TypeDefinition`, `Base`, `Derived`,
    ///   `Constructor`, and `Destructor`:
    ///   - `data[0]`: The target symbol.
    ///   - `data[1]`: Empty (unused).
    ///
    /// - `Caller` and `Callee`:
    ///   - `data[0]`: The target symbol (e.g., the called function).
    ///   - `data[1]`: The range of the call site.
    ///
    ValueRef data[2];
};

struct Symbol {
    uint64_t id;
    String name;
    SymbolKind kind;
    Array<Relation> relations;
};

struct Occurrence {
    ValueRef location;
    ValueRef symbol;
};

struct SymbolIndex {
    Array<Symbol> symbols;
    Array<Occurrence> occurrences;
    Array<LocalSourceRange> ranges;
};

struct DocumentLink {
    String file;
    ValueRef range;
};

struct Hover {
    String contents;
    ValueRef range;
};

struct FoldingRange {
    ValueRef ranges;
    /// FIXME: support labels.
};

struct DocumentSymbol {
    String name;
    SymbolKind kind;
    ValueRef range;
    ValueRef selection;
    Array<DocumentSymbol> children;
};

struct SemanticToken {
    SymbolKind kind;
    ValueRef range;
};

struct InlayHint {
    String label;
    ValueRef range;
    /// FIXME: support parts.
};

struct FeatureIndex {
    Array<DocumentLink> links;
    Array<Hover> hovers;
    Array<FoldingRange> folds;
    Array<DocumentSymbol> symbols;
    Array<SemanticToken> tokens;
    Array<InlayHint> hints;
    Array<LocalSourceRange> ranges;
};
