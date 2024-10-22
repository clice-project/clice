#pragma once

#include <Index/Index.h>
#include <Compiler/Clang.h>

namespace clice::index::in {

class Indexer {
public:
    Indexer(clang::Sema& sema, clang::syntax::TokenBuffer& tokBuf) : sema(sema), tokBuf(tokBuf) {}

    Index index();

    std::size_t lookup(const clang::NamedDecl* decl);

    Indexer& addSymbol(const clang::NamedDecl* decl);

    Indexer& addOccurrence(int Kind, clang::SourceLocation location);

    Indexer& addOccurrence(const clang::NamedDecl* decl, clang::SourceRange range);

    Indexer& addRelation(const clang::NamedDecl* from,
                         clang::SourceRange range,
                         std::initializer_list<RelationKind> roles);

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

}  // namespace clice::index::in

