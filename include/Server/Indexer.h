#pragma once

#include <vector>
#include "Async/Async.h"
#include "AST/SymbolID.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/StringMap.h"
#include "Index/Index.h"

namespace clice {

class ASTInfo;

class Indexer {
public:
    /// Index an opened file, its AST is already builtin
    /// and PCH is used for it.
    async::Task<> index(ASTInfo& AST);

    /// Index an static file.
    async::Task<> index(llvm::StringRef file);

public:

private:
    using Path = std::string;
    using PathID = std::uint32_t;
    using SymbolID = std::uint64_t;
    using SymbolIndex = std::unique_ptr<index::memory2::SymbolIndex>;

    /// All paths of indices.
    std::vector<Path> path_storage;

    /// A map between path and its id.
    llvm::StringMap<PathID> paths;

    /// A map between source file path and its static indices.
    llvm::DenseMap<PathID, Path> static_indices;

    /// A map between symbol id and files that contains it.
    llvm::DenseMap<SymbolID, llvm::DenseSet<PathID>> symbol_indices;

    /// A map between source file path and its dynamic indices.
    llvm::DenseMap<PathID, std::vector<SymbolIndex>> dynamic_indices;
};

}  // namespace clice
