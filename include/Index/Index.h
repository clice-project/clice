#pragma once

#include "Relation.h"

namespace clice::index {

/// Represent a position in the source code, the line and column are 1-based.
struct Position {
    /// The line of the position.
    uint32_t line = 0;

    /// The column of the position.
    uint32_t column = 0;

    friend constexpr auto operator<=> (const Position& lhs, const Position& rhs) = default;
};

struct Location {
    /// Begin position of the location.
    Position begin = {};

    /// End position of the location.
    Position end = {};

    /// The index of the file in the `Index::files`. When the value of file is equal to MAX_UINT32
    /// it means the location is invalid.
    uint32_t file = std::numeric_limits<uint32_t>::max();

    bool isValid() const {
        return file != std::numeric_limits<uint32_t>::max();
    }

    explicit operator bool () const {
        return isValid();
    }

    friend constexpr auto operator<=> (const Location& lhs, const Location& rhs) = default;
};

struct Occurrence {
    /// The index of the location in `Index::locations`.
    uint32_t location;

    /// The index of the symbol in `Index::symbols`.
    uint32_t symbol;
};

template <typename String>
struct File {
    /// The path of the file.
    String path;

    /// FIXME: Add some flags.

    /// The index of include location of this file if any.
    uint32_t include;
};

template <typename String, template <typename...> typename Array>
struct Symbol {
    /// The hash of this symbol, used for quick lookup.
    uint64_t id;

    /// The unique identifier of this symbol.
    String USR;

    /// The name of this symbol.
    String name;

    /// The document of this symbol.
    String document;

    bool is_local;
    // FIXME: ... more useful fields.

    /// The relations of this symbol.
    Array<Relation> relations;
};

template <typename String, template <typename...> typename Array>
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
    Array<File<String>> files;

    /// All the symbols in the source file, sorted by `Symbol::SymbolID`.
    Array<Symbol<String, Array>> symbols;

    /// All the occurrences in the source file, sorted by `Occurrence::Location`.
    Array<Occurrence> occurrences;

    /// Cache same locations to reduce the size of the index.
    Array<Location> locations;
};

}  // namespace clice::index
