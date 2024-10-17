#pragma once

#include <Index/CSIF.h>

namespace clice {

class SymbolSlab {
public:
    SymbolSlab& addSymbol(const clang::Decl* decl);

    SymbolSlab& addOccurrence(const clang::Decl* decl, protocol::Range range, Role role);

    SymbolSlab& addRelation(const clang::Decl* from, const clang::Decl* to, Role role);

    std::size_t lookup(const clang::Decl* decl) {
        auto iter = cache.find(decl);
        if(iter != cache.end()) {
            return iter->second;
        }

        llvm::outs() << "SymbolSlab::lookup: decl not found\n";
        std::terminate();
    }

    CSIF index(clang::ASTContext& context);

private:
    llvm::BumpPtrAllocator allocator;
    llvm::StringSaver saver{allocator};

    std::vector<Symbol> symbols;
    std::vector<Occurrence> occurrences;
    std::vector<std::vector<Relation>> relations;
    llvm::DenseMap<const clang::Decl*, std::size_t> cache;
};

}  // namespace clice

