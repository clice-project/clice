#pragma once

#include <Support/ADT.h>
#include <Protocol/Basic.h>

namespace clice::protocol {

struct Symbol;
struct Occurrence;
struct Diagnostic;
struct InlayHint;

/// CSIF stands for "C/C++ Semantic Index Format".
/// It is an efficient binary format for storing the semantic information of C/C++ source code.
/// The main references are [SCIP](https://sourcegraph.com/blog/announcing-scip) and
/// [SemanticDB](https://scalameta.org/docs/semanticdb/specification.html).
struct CSIF {
    /// The version of the CSIF format.
    StringRef version = "0.0.1";
    /// The language of the source code, currently only supports "c" and "c++".
    StringRef language = "c++";
    /// The URI of the source file.
    StringRef uri;
    /// The context of the source file, used to check whether need to re-index the source file.
    StringRef content;
    /// The commands used to compile the source file.
    ArrayRef<StringRef> commands;

    /// The symbols in the source file.
    ArrayRef<Symbol> symbols;
    /// The occurrences in the source file.
    ArrayRef<Occurrence> occurrences;
    /// The diagnostics in the source file.
    ArrayRef<Diagnostic> diagnostics;
    /// The semantic tokens in the source file.
    ArrayRef<std::uint32_t> semanticTokens;
    /// The inlay hints in the source file.
    ArrayRef<InlayHint> inlayHints;
};

struct SymbolID {};

enum Role {
    Declaration,
    Definition,
    Reference,
    TypeDefinition,
    Base,
    Override,
    Write,
    Read,
    // TODO:
};

struct Relation {
    /// The target of the relation. For declaration and definition, it's empty.
    /// For reference, it's the ID of the symbol which references.
    SymbolID target;
    /// The role of the relation.
    Role role;
    /// The range of the target, maybe empty.
    Range range;
};

struct Symbol {
    /// The ID of the symbol.
    SymbolID id;
    /// display when hover.
    StringRef document;

    // TODO: append more useful information.

    /// The relations of the symbol.
    ArrayRef<Relation> relations;
};

struct Occurrence {
    /// The ID of the symbol.
    SymbolID symbol;
    /// The range of the occurrence.
    Range range;
    /// The role of the occurrence.
    Role role;
};

}  // namespace clice::protocol
