#include <Index/SymbolSlab.h>
#include <clang/Index/USRGeneration.h>

namespace clice {

std::size_t SymbolSlab::lookup(const clang::NamedDecl* decl) {
    auto iter = cache.find(decl);
    if(iter == cache.end()) {
        llvm::outs() << "SymbolSlab::lookup: decl not found\n";
        std::terminate();
    }
    return iter->second;
}

SymbolSlab& SymbolSlab::addSymbol(const clang::NamedDecl* decl) {
    // Generate and save USR.
    llvm::SmallString<128> USR;
    clang::index::generateUSRForDecl(decl, USR);

    if(!symbolIndex.contains(SymbolID::fromUSR(USR))) {
        auto ID = SymbolID::fromUSR(saver.save(USR.str()));
        symbols.emplace_back(ID);
        cache.try_emplace(decl, symbols.size() - 1);
        symbolIndex.try_emplace(ID, symbols.size() - 1);
    } else {
        cache.try_emplace(decl, symbolIndex[SymbolID::fromUSR(USR)]);
    }

    return *this;
}

SymbolSlab& SymbolSlab::addOccurrence(const clang::NamedDecl* decl, clang::SourceLocation range, Role role) {
    auto& srcMgr = sema.getSourceManager();
    auto ID = symbols[lookup(decl)].ID;
    auto line = srcMgr.getPresumedLineNumber(range);
    auto column = srcMgr.getPresumedColumnNumber(range);
    occurrences.emplace_back(Occurrence{ID, line, column, line, column, role});
    return *this;
}

SymbolSlab& SymbolSlab::addOccurrence(const clang::NamedDecl* decl, clang::SourceRange range, Role role) {
    auto ID = symbols[lookup(decl)].ID;
    auto& srcMgr = sema.getSourceManager();
    auto line = srcMgr.getPresumedLineNumber(range.getBegin());
    auto column = srcMgr.getPresumedColumnNumber(range.getBegin());
    occurrences.emplace_back(Occurrence{ID, line, column, line, column, role});
    return *this;
}

SymbolSlab& SymbolSlab::addOccurrence(int Kind, clang::SourceLocation loc) {
    auto ID = SymbolID::fromKind(Kind);
    // occurrences.emplace_back(Occurrence{ID, range, Role::Reference});
    return *this;
}

SymbolSlab& SymbolSlab::addRelation(const clang::NamedDecl* from, const clang::NamedDecl* to, Role role) {
    std::size_t index = lookup(from);
    SymbolID fromID = symbols[index].ID;
    SymbolID toID = symbols[lookup(to)].ID;
    relations[index].emplace_back(Relation{toID, role});
    return *this;
}

}  // namespace clice
