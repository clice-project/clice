#ifndef MAKE_CLANG_HAPPY

#include "Index.h"

namespace clice::index {

struct String {};

template <typename T>
struct Array {};

}  // namespace clice::index

using namespace clice::index;

#undef MAKE_CLANG_HAPPY
#endif

struct File {
    /// The path of the file.
    String path;

    /// The include location of this file if any.
    LocationRef include;
};

struct Symbol {
    /// The hash of this symbol, used for quick lookup.
    uint64_t id;

    /// The name of this symbol.
    String name;

    /// The relations of this symbol.
    Array<Relation> relations;
};

struct Index {
    /// The version of the index format.
    String version;

    /// The language of the indexed code, currently only supports "C" and "C++".
    String language;

    /// The filepath of the source file.
    String path;

    /// The content of the source file.
    String content;

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

