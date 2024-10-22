#include <Index/Indexer.h>
#include <clang/Index/USRGeneration.h>

namespace clice::index::in {

namespace {

Location toRange(clang::SourceRange range, clang::syntax::TokenBuffer& tokBuf) {
    auto& srcMgr = tokBuf.sourceManager();
    Location location{};
    location.file = srcMgr.getFilename(range.getBegin());
    /// It's impossible that a range has crossed multiple files.
    assert(location.file == srcMgr.getFilename(range.getEnd()));

    if(range.getBegin().isMacroID() || range.getEnd().isMacroID()) {
        range.dump(srcMgr);
        return location;
    }

    auto begin = tokBuf.spelledTokenContaining(range.getBegin());
    auto end = tokBuf.spelledTokenContaining(range.getEnd());

    if(!begin || !end) {
        // FIXME:
        std::terminate();
    }

    location.begin.line = srcMgr.getPresumedLineNumber(begin->location());
    location.begin.column = srcMgr.getPresumedColumnNumber(begin->location());

    location.end.line = srcMgr.getPresumedLineNumber(end->endLocation());
    location.end.column = srcMgr.getPresumedColumnNumber(end->endLocation());

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

    if(!symbolIndex.contains(USRToSymbolID(USR))) {
        auto ID = USRToSymbolID(saver.save(USR.str()));
        symbols.emplace_back(ID);
        Symbol symbol;
        symbols.back().document = saver.save(decl->getNameAsString());
        cache.try_emplace(decl, symbols.size() - 1);
        symbolIndex.try_emplace(ID, symbols.size() - 1);
        relations.emplace_back();
    } else {
        cache.try_emplace(decl, symbolIndex[USRToSymbolID(USR)]);
    }

    return *this;
}

Indexer& Indexer::addOccurrence(const clang::NamedDecl* decl, clang::SourceRange range) {
    if(range.isInvalid()) {
        return *this;
    }

    auto ID = symbols[lookup(decl)].ID;
    auto& srcMgr = sema.getSourceManager();
    occurrences.emplace_back(Occurrence{ID, toRange(range, tokBuf)});
    return *this;
}

Indexer& Indexer::addOccurrence(int Kind, clang::SourceLocation loc) {
    if(loc.isInvalid()) {
        return *this;
    }

    auto ID = kindToSymbolID(Kind);
    occurrences.emplace_back(Occurrence{ID, toRange(loc, tokBuf)});
    return *this;
}

Indexer& Indexer::addRelation(const clang::NamedDecl* from,
                              clang::SourceRange range,
                              std::initializer_list<RelationKind> roles) {
    if(range.isInvalid()) {
        return *this;
    }

    auto index = lookup(from);
    auto& relations = this->relations[index];
    for(auto role: roles) {
        relations.emplace_back(Relation{role, toRange(range, tokBuf)});
    }
    return *this;
}

}  // namespace clice::index::in
