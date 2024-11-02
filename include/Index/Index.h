#ifndef MAKE_CLANGD_HAPPY
/// This file is non self contained, which clangd supports.
/// So made up some types to make clangd happy.

#include <cstdint>
#include <limits>

enum RelationKind {};

struct Position {};

struct Location {};

template <typename T>
using Value = T;

template <typename T>
struct Array {};

using String = Array<char>;

#endif

#undef MAKE_CLANGD_HAPPY

struct File {
    String path;
    // TODO: some flags.
    /// Include location of the file if any.
    Value<Location> include;
};

struct SymbolID {
    uint64_t value;
    String USR;
};

struct Relation {
    RelationKind kind;
    Value<Location> location;
    Value<SymbolID> related;
};

struct Symbol {
    Value<SymbolID> id;
    /// The name of this symbol.
    String name;
    /// The document of this symbol.
    String document;
    // TODO: ... more useful fields.
    /// The relations of this symbol.
    Array<Relation> relations;
};

struct Occurrence {
    Value<Location> location;
    /// The index of the symbol in `Index::symbols`.
    uint32_t symbol;
};

struct Index {
    /// The version of the index format.
    String version;
    /// The language of the indexed code, currently only supports "C" and "C++".
    String language;
    /// The filepath of the source file.
    String path;
    /// The context of the source file.
    String context;
    /// The commands used to compile the source file.
    Array<String> commands;
    /// All files occurred in compilation.
    Array<File> files;
    /// All the symbols in the source file, sorted by `Symbol::SymbolID`.
    Array<Symbol> symbols;
    /// All the occurrences in the source file, sorted by `Occurrence::Location`.
    Array<Occurrence> occurrences;
};

