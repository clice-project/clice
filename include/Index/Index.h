#pragma once

#include <Support/ADT.h>
#include <Protocol/Basic.h>

#include <Index/SymbolID.h>

namespace clice {

struct Symbol;
struct Occurrence;

// struct Diagnostic {};
// struct InlayHint {};

/// CSIF stands for "C/C++ Semantic Index Format".
/// It is an efficient binary format for storing the semantic information of C/C++ source code.
/// The main references are [SCIP](https://sourcegraph.com/blog/announcing-scip) and
/// [SemanticDB](https://scalameta.org/docs/semanticdb/specification.html).
struct CSIF {
    /// The version of the CSIF format.
    llvm::StringRef version;
    /// The language of the source code, currently only supports "c" and "c++".
    llvm::StringRef language;
    /// The URI of the source file.
    llvm::StringRef uri;
    /// The context of the source file, used to check whether need to re-index the source file.
    llvm::StringRef content;
    /// The commands used to compile the source file.
    llvm::ArrayRef<llvm::StringRef> commands;

    /// The symbols in the source file.
    llvm::ArrayRef<Symbol> symbols;
    /// The occurrences in the source file.
    llvm::ArrayRef<Occurrence> occurrences;

    ///// The semantic tokens in the source file.
    // llvm::ArrayRef<std::uint32_t> semanticTokens;

    // FIXME:
    /// The diagnostics in the source file.
    // llvm::ArrayRef<Diagnostic> diagnostics;
    /// The inlay hints in the source file.
    // llvm::ArrayRef<InlayHint> inlayHints;
};

/// Note that it's possible to have multiple roles at the same time.
enum class Role {
    Invalid,
    Declaration,
    Definition,
    Reference,
    // Write Relation.
    Read,
    Write,
    Interface,
    Implementation,
    /// When target is a type definition of source, source is possible type or constructor.
    TypeDefinition,

    /// When target is a base class of source.
    Base,
    /// When target is a derived class of source.
    Derived,

    /// When target is a constructor of source.
    Constructor,
    /// When target is a destructor of source.
    Destructor,

    /// When target is a partial specialization of source.
    PartialSpecialization,
    /// When target is a full specialization of source.
    FullSpecialization,
    /// When target is an explicit instantiation of source.
    ImplicitInstantiation,

    // When target is a caller of source.
    Caller,
    // When target is a callee of source.
    Callee,
};

struct Location {
    proto::DocumentUri uri;
    proto::Range range;

    friend std::strong_ordering operator<=> (const Location& lhs, const Location& rhs) = default;
};

/// If symbol A has a relation to symbol B with role R.
/// For example, `Caller`. Then we say B is a caller of A.
struct Relation {
    /// The role of the relation.
    Role role;
    /// The location of the related symbol.
    Location location;

    friend std::strong_ordering operator<=> (const Relation& lhs, const Relation& rhs) = default;
};

struct Symbol {
    /// The ID of the symbol.
    SymbolID ID;
    /// display when hover.
    llvm::StringRef document;

    // TODO: append more useful information.

    /// The relations of the symbol.
    llvm::ArrayRef<Relation> relations;
};

struct Occurrence {
    /// The ID of the symbol.
    SymbolID symbol;
    /// The range of the occurrence.
    Location location;
};

enum BuiltinSymbolKind {

#define SYMBOL(name, description) name,
#include <Index/Symbols.def>
#undef SYMBOL
};

}  // namespace clice
