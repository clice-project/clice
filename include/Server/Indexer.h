#pragma once

#include <vector>
#include "Async/Async.h"
#include "AST/SymbolID.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/StringMap.h"
#include "Compiler/Command.h"
#include "Index/Index.h"

namespace clice {

class CompilationUnit;

class Indexer {
public:
    /// Index an opened file, its AST is already builtin
    /// and PCH is used for it.
    async::Task<> index(CompilationUnit& unit);

    /// Index an static file.
    async::Task<> index(llvm::StringRef file);

    using Path = std::string;
    using PathID = std::uint32_t;
    using SymbolID = std::uint64_t;

public:
    Indexer(CompilationDatabase& database) : database(database) {}

    PathID getPath(llvm::StringRef path) {
        auto it = paths.find(path);
        if(it != paths.end()) {
            return it->second;
        }

        auto id = path_storage.size();
        path_storage.emplace_back(path);
        paths.try_emplace(path, id);
        return id;
    }

private:
    struct HeaderIndices {
        using RawIndex = std::pair<std::uint32_t, std::unique_ptr<index::memory::RawIndex>>;

        /// The merged index.
        std::unique_ptr<index::memory::HeaderIndex> merged;

        llvm::DenseMap<PathID, std::vector<RawIndex>> unmergeds;
    };

    CompilationDatabase& database;

    /// All paths of indices.
    std::vector<Path> path_storage;

    /// A map between path and its id.
    llvm::StringMap<PathID> paths;

    /// A map between symbol id and files that contains it.
    llvm::DenseMap<SymbolID, llvm::DenseSet<PathID>> symbol_indices;

    /// A map between source file path and its static indices.
    llvm::DenseMap<PathID, Path> static_indices;

    std::uint32_t unmerged_count = 0;

    /// In-memory header indices.
    llvm::DenseMap<PathID, std::unique_ptr<HeaderIndices>> dynamic_header_indices;

    /// In-memory translation unit indices.
    llvm::DenseMap<PathID, std::unique_ptr<index::memory::TUIndex>> dynamic_tu_indices;
};

}  // namespace clice
