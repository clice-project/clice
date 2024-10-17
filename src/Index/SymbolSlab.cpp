#include <Index/SymbolSlab.h>
#include <clang/Index/USRGeneration.h>

namespace clice {

std::size_t SymbolSlab::lookup(const clang::Decl* decl) {
    auto iter = cache.find(decl);
    if(iter == cache.end()) {
        llvm::outs() << "SymbolSlab::lookup: decl not found\n";
        std::terminate();
    }
    return iter->second;
}

SymbolSlab& SymbolSlab::addSymbol(const clang::Decl* decl) {
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

SymbolSlab& SymbolSlab::addOccurrence(const clang::Decl* decl, protocol::Range range, Role role) {
    auto ID = symbols[lookup(decl)].ID;
    occurrences.emplace_back(Occurrence{ID, range, role});
    return *this;
}

SymbolSlab& SymbolSlab::addRelation(const clang::Decl* from, const clang::Decl* to, Role role) {
    std::size_t index = lookup(from);
    SymbolID fromID = symbols[index].ID;
    SymbolID toID = symbols[lookup(to)].ID;
    relations[index].emplace_back(Relation{toID, role});
    return *this;
}

}  // namespace clice
