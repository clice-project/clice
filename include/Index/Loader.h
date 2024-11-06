#pragma once

#include "Serialize.h"

namespace clice::index {

/// A helper class to load the index from a binary file and provide some utility functions.
class Loader {
public:
    Loader(std::vector<char>&& binary) :
        binary(std::move(binary)),
        index(*reinterpret_cast<const binary::Index*>(this->binary.data())) {}

    template <typename T>
    auto make_range(binary::array<T> array) const {
        auto begin = reinterpret_cast<const T*>(binary.data() + array.offset);
        return llvm::ArrayRef(begin, begin + array.size);
    }

    llvm::ArrayRef<binary::File> files() const {
        return make_range(index.files);
    }

    llvm::ArrayRef<binary::Symbol> symbols() const {
        return make_range(index.symbols);
    }

    llvm::ArrayRef<Occurrence> occurrences() const {
        return make_range(index.occurrences);
    }

    llvm::ArrayRef<Location> locations() const {
        return make_range(index.locations);
    }

    const binary::File& file(FileRef file) const {
        return files()[file.offset];
    }

    const binary::Symbol& symbol(SymbolRef sym) const {
        return symbols()[sym.offset];
    }

    const Location& location(LocationRef loc) const {
        return locations()[loc.offset];
    }

    llvm::StringRef string(binary::string str) const {
        return llvm::StringRef(binary.data() + str.offset, str.size);
    }

    Position begin(LocationRef loc) const {
        return location(loc).begin;
    }

    Position end(LocationRef loc) const {
        return location(loc).end;
    }

    llvm::StringRef filepath(LocationRef loc) const {
        return string(file(location(loc).file).path);
    }

    LocationRef includeLoc(FileRef file) const {
        return this->file(file).include;
    }

    LocationRef includeLoc(LocationRef loc) const {
        return includeLoc(location(loc).file);
    }

    /// Locate the symbol at the given position.
    const binary::Symbol& locateSymbol(FileRef file, Position pos) {
        /// We use the default `<=>` operator when sorting the occurrences.
        /// And file is the first key, so locations in the same file are contiguous.
        /// Then we can use binary search to locate the file first, then search the occurrences in
        /// the file.
        auto fileStart = std::lower_bound(occurrences().begin(),
                                          occurrences().end(),
                                          file,
                                          [&](Occurrence occurrence, FileRef file) {
                                              return location(occurrence.location).file < file;
                                          });

        if(fileStart == occurrences().end() || location(fileStart->location).file != file) {
            /// FIXME: add error handle.
            std::terminate();
        }

        auto occurrence = std::lower_bound(fileStart,
                                           occurrences().end(),
                                           pos,
                                           [&](const Occurrence& occurrence, const Position& pos) {
                                               return end(occurrence.location) < pos;
                                           });

        if(occurrence == occurrences().end() || begin(occurrence->location) > pos) {
            /// FIXME: add error handle.
            std::terminate();
        }

        return symbol(occurrence->symbol);
    }

    /// Locate the symbol with the given id.
    llvm::ArrayRef<binary::Symbol> locateSymbol(uint64_t id) {
        struct Compare {
            bool operator() (const binary::Symbol& symbol, uint64_t id) const {
                return symbol.id < id;
            }

            bool operator() (uint64_t id, const binary::Symbol& symbol) const {
                return id < symbol.id;
            }
        };

        auto range = std::equal_range(symbols().begin(), symbols().end(), id, Compare{});
        return {range.first, range.second};
    }

    /// Locate the file with the given path.
    llvm::ArrayRef<binary::File> locateFile(llvm::StringRef path) {
        struct Compare {
            Loader& loader;

            bool operator() (const binary::File& file, llvm::StringRef path) const {
                return loader.string(file.path) < path;
            }

            bool operator() (llvm::StringRef path, const binary::File& file) const {
                return path < loader.string(file.path);
            }
        };

        auto range = std::equal_range(files().begin(), files().end(), path, Compare{*this});
        return {range.first, range.second};
    }

private:
    std::vector<char> binary;
    const binary::Index& index;
};

}  // namespace clice::index
