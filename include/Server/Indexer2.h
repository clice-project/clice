#pragma once

#include <vector>
#include "AST/SymbolID.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/StringMap.h"

namespace clice {

class Indexer {
public:

private:
    /// All paths in indexes.
    std::vector<std::string> paths;

    /// A map between path and its index.
    llvm::StringMap<std::uint32_t> pathIndex;

    /// A map between source file path and its static index file.
    llvm::DenseMap<std::uint32_t, std::string> mapToIndex;

    /// A map between symbol id and files that contains it.
    llvm::DenseMap<std::uint64_t, std::vector<std::uint32_t>> invertedSymbolMap;
};

}  // namespace clice
