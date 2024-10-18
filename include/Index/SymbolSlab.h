#pragma once

#include <Index/CSIF.h>
#include <Compiler/Clang.h>

namespace clice {

class SymbolSlab {
public:
    SymbolSlab(clang::Sema& sema, clang::syntax::TokenBuffer& tokBuf) : sema(sema), tokBuf(tokBuf) {}

    CSIF index();

    std::size_t lookup(const clang::NamedDecl* decl);

    SymbolSlab& addSymbol(const clang::NamedDecl* decl);

    SymbolSlab& addOccurrence(int Kind, clang::SourceLocation location);

    SymbolSlab& addOccurrence(const clang::NamedDecl* decl, clang::SourceLocation location);

    SymbolSlab& addOccurrence(const clang::NamedDecl* decl, clang::SourceRange range);

    SymbolSlab& addRelation(const clang::NamedDecl* from, clang::SourceLocation location, std::initializer_list<Role> roles);

    SymbolSlab& addRelation(const clang::NamedDecl* from, clang::SourceRange range, std::initializer_list<Role> roles);

private:
    clang::Sema& sema;
    clang::syntax::TokenBuffer& tokBuf;

    llvm::BumpPtrAllocator allocator;
    llvm::StringSaver saver{allocator};

    std::vector<Symbol> symbols;
    std::vector<Occurrence> occurrences;
    std::vector<std::vector<Relation>> relations;

    llvm::DenseMap<SymbolID, std::size_t> symbolIndex;
    llvm::DenseMap<const clang::Decl*, std::size_t> cache;
};

}  // namespace clice

