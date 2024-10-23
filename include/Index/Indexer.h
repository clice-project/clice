#pragma once

#include <Index/Index.h>
#include <Compiler/Clang.h>

namespace clice::index {

class Indexer {
public:
    Indexer(clang::Sema& sema, clang::syntax::TokenBuffer& tokBuf) : sema(sema), tokBuf(tokBuf) {}

    in::Index index();

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

    std::vector<in::Symbol> symbols;
    std::vector<in::Occurrence> occurrences;
    std::vector<std::vector<in::Relation>> relations;

    llvm::DenseMap<in::SymbolID, std::size_t> symbolIndex;
    llvm::DenseMap<const clang::Decl*, std::size_t> cache;
};

}  // namespace clice::index

