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

using SymbolID = uint64_t;

struct File {
    String path;
    // TODO: some flags.
    /// Include location of the file if any.
    uint32_t include;
};

struct Relation {
    RelationKind kind;
    uint32_t location;
    /// An extra file used to provide extra information, according to the `kind`.
    /// e.g. for Definition, this is used to store the location of whole block.
    /// Above `location` is just the location of the name.
    /// For `Caller` and `Callee`, this is used to store the index of target symbol.
    uint32_t symOrLoc;
};

struct Symbol {
    /// The hash of this symbol, used for quick lookup.
    SymbolID id;
    /// The unique identifier of this symbol.
    String USR;
    /// The name of this symbol.
    String name;
    /// The document of this symbol.
    String document;
    // TODO: ... more useful fields.
    /// The relations of this symbol.
    Array<Relation> relations;
};

struct Occurrence {
    uint32_t location;
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
    /// Cache same locations to reduce the size of the index.
    Array<Location> locations;
};

