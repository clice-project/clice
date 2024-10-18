#pragma once

#include <Index/CSIF.h>
#include <Compiler/Clang.h>

namespace clice {

class SymbolSlab {
public:
    CSIF index(clang::Sema& sema, clang::syntax::TokenBuffer& tokBuf);

    std::size_t lookup(const clang::Decl* decl);

    SymbolSlab& addSymbol(const clang::Decl* decl);

    SymbolSlab& addOccurrence(const clang::Decl* decl, proto::Range range, Role role);

    SymbolSlab& addOccurrence(int Kind, proto::Range range);

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

