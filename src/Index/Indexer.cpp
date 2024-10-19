#include <Index/Indexer.h>
#include <clang/Index/USRGeneration.h>

namespace clice {

namespace {

Location toLocation(clang::SourceRange loc, clang::SourceManager& srcMgr) {
    Location location;
    location.uri = srcMgr.getFilename(loc.getBegin());
    location.range.start.line = srcMgr.getPresumedLineNumber(loc.getBegin());
    location.range.start.character = srcMgr.getPresumedColumnNumber(loc.getBegin());
    location.range.end.line = srcMgr.getPresumedLineNumber(loc.getEnd());
    location.range.end.character = srcMgr.getPresumedColumnNumber(loc.getEnd());
    return location;
}

}  // namespace

std::size_t Indexer::lookup(const clang::NamedDecl* decl) {
    auto iter = cache.find(decl);
    if(iter != cache.end()) {
        return iter->second;
    }

    addSymbol(decl);
    return symbols.size() - 1;
}

Indexer& Indexer::addSymbol(const clang::NamedDecl* decl) {
    // Generate and save USR.
    llvm::SmallString<128> USR;
    clang::index::generateUSRForDecl(decl, USR);

    if(!symbolIndex.contains(SymbolID::fromUSR(USR))) {
        auto ID = SymbolID::fromUSR(saver.save(USR.str()));
        symbols.emplace_back(ID);
        symbols.back().document = saver.save(decl->getNameAsString());

        cache.try_emplace(decl, symbols.size() - 1);
        symbolIndex.try_emplace(ID, symbols.size() - 1);
        relations.emplace_back();
    } else {
        cache.try_emplace(decl, symbolIndex[SymbolID::fromUSR(USR)]);
    }

    return *this;
}

Indexer& Indexer::addOccurrence(const clang::NamedDecl* decl, clang::SourceRange range) {
    if(range.isInvalid()) {
        return *this;
    }

    auto ID = symbols[lookup(decl)].ID;
    auto& srcMgr = sema.getSourceManager();
    occurrences.emplace_back(Occurrence{ID, toLocation(range.getBegin(), srcMgr)});
    return *this;
}

Indexer& Indexer::addOccurrence(int Kind, clang::SourceLocation loc) {
    if(loc.isInvalid()) {
        return *this;
    }

    auto ID = SymbolID::fromKind(Kind);
    occurrences.emplace_back(Occurrence{ID, toLocation(loc, sema.getSourceManager())});
    return *this;
}

Indexer& Indexer::addRelation(const clang::NamedDecl* from,
                              clang::SourceRange range,
                              std::initializer_list<Role> roles) {
    if(range.isInvalid()) {
        return *this;
    }

    auto index = lookup(from);
    auto& relations = this->relations[index];
    for(auto role: roles) {
        relations.emplace_back(Relation{role, toLocation(range, sema.getSourceManager())});
    }
    return *this;
}

}  // namespace clice
