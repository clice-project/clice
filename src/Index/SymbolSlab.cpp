#include <Index/SymbolSlab.h>
#include <clang/Index/USRGeneration.h>

namespace clice {

SymbolSlab& SymbolSlab::addSymbol(const clang::Decl* decl) {
    // Generate and save USR.
    llvm::SmallString<128> USR;
    clang::index::generateUSRForDecl(decl, USR);
    saver.save(USR.str());

    if(cache.contains(decl)) {
        llvm::outs() << "SymbolSlab::addSymbol: decl already exists\n";
        std::terminate();
    }

    auto index = symbols.size();
    symbols.emplace_back(SymbolID::fromUSR(USR.str()));
    cache.try_emplace(decl, index);
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
