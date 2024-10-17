#pragma once

#include <Index/CSIF.h>

namespace clice {

class SymbolSlab {
public:
    CSIF index(clang::ASTContext& context);

    std::size_t lookup(const clang::Decl* decl);

    SymbolSlab& addSymbol(const clang::Decl* decl);

    SymbolSlab& addOccurrence(const clang::Decl* decl, protocol::Range range, Role role);

    SymbolSlab& addRelation(const clang::Decl* from, const clang::Decl* to, Role role);

private:
    llvm::BumpPtrAllocator allocator;
    llvm::StringSaver saver{allocator};

    std::vector<Symbol> symbols;
    std::vector<Occurrence> occurrences;
    std::vector<std::vector<Relation>> relations;

    llvm::DenseMap<SymbolID, std::size_t> symbolIndex;
    llvm::DenseMap<const clang::Decl*, std::size_t> cache;
};

}  // namespace clice

