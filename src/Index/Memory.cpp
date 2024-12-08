#include "Index/Memory.h"
#include <ranges>

namespace clice::index {

namespace {

const static clang::NamedDecl* normalize(const clang::NamedDecl* decl) {
    if(!decl) {
        std::terminate();
    }

    decl = llvm::cast<clang::NamedDecl>(decl->getCanonicalDecl());

    if(auto ND = instantiatedFrom(llvm::cast<clang::NamedDecl>(decl))) {
        return llvm::cast<clang::NamedDecl>(ND->getCanonicalDecl());
    }

    return decl;
}

class IndexBuilder : public SemanticVisitor<IndexBuilder> {
public:
    IndexBuilder(ASTInfo& info, memory::Index& index) : SemanticVisitor(info), index(&index) {}

    /// Add a file to the index.
    FileRef addFile(clang::FileID id) {
        auto& files = index->files;

        /// Try to insert the file into the cache.
        auto [iter, success] = fileCache.try_emplace(id, files.size());
        uint32_t offset = iter->second;

        /// If insertion is successful, add the file to the index.
        if(success) {
            files.emplace_back();

            /// FIXME: handle error.
            auto entry = srcMgr.getFileEntryRefForID(id);
            if(!entry) {
                llvm::errs() << "File entry not found\n";
                std::terminate();
            }

            /// FIXME: resolve the real path(consider real path).
            // llvm::SmallString<128> path;
            // if(auto error = fs::real_path(entry->getName(), path)) {
            //     llvm::errs() << error.message() << "\n";
            //     std::terminate();
            // }

            auto include = addLocation(srcMgr.getIncludeLoc(id));

            /// Note that addLocation may invalidate the reference to files.
            /// So we use the offset to access the file.
            files[offset].path = entry->getName();
            files[offset].include = include;
        }

        return FileRef{offset};
    }

    /// Add a symbol to the index.
    LocationRef addLocation(clang::SourceRange range) {
        auto [begin, end] = range;

        /// FIXME: handle macro locations.
        if(begin.isInvalid() || end.isInvalid() || begin.isMacroID() || end.isMacroID()) {
            return LocationRef{};
        }

        /// Try to insert the location into the cache.
        auto [iter, success] = locationCache.try_emplace({begin, end}, index->locations.size());
        uint32_t offset = iter->second;

        /// If insertion is successful, add the location to the index.
        if(success) {
            auto& locations = index->locations;
            locations.emplace_back();

            auto beginLoc = srcMgr.getPresumedLoc(begin);
            auto endTok = clang::Lexer::getLocForEndOfToken(end, 0, srcMgr, sema.getLangOpts());
            auto endLoc = srcMgr.getPresumedLoc(endTok);

            /// FIXME: position encoding.
            locations[offset].range.start.line = beginLoc.getLine() - 1;
            locations[offset].range.start.character = beginLoc.getColumn() - 1;
            locations[offset].range.end.line = endLoc.getLine() - 1;
            locations[offset].range.end.character = endLoc.getColumn() - 1;
            locations[offset].file = addFile(srcMgr.getFileID(begin));
        }

        return LocationRef{offset};
    }

    /// Add a symbol to the index.
    SymbolRef addSymbol(const clang::NamedDecl* decl) {
        decl = normalize(decl);
        auto& symbols = index->symbols;
        auto canonical = decl->getCanonicalDecl();

        /// Try to insert the symbol into the cache.
        auto [iter, success] = symbolCache.try_emplace(canonical, symbols.size());
        uint32_t offset = iter->second;

        /// If insertion is successful, add the symbol to the index.
        if(success) {
            memory::Symbol& symbol = symbols.emplace_back();
            llvm::SmallString<128> USR;
            /// clang::index::generateUSRForDecl(canonical, USR);
            symbol.id = llvm::xxHash64(USR);
            symbol.name = decl->getNameAsString();
        }

        return SymbolRef{offset};
    }

    void addRelation(SymbolRef symbol, RelationKind kind, LocationRef location) {
        index->symbols[symbol.offset].relations.emplace_back(kind, location);
    }

    void addOccurrence(LocationRef location, SymbolRef symbol) {
        index->occurrences.emplace_back(location, symbol);
    }

public:
    using SemanticVisitor::handleOccurrence;

    void handleOccurrence(const clang::Decl* decl, clang::SourceRange range, RelationKind kind) {
        auto symbolRef = addSymbol(llvm::cast<clang::NamedDecl>(decl));
        auto [begin, end] = range;
        if(kind == RelationKind::Declaration || kind == RelationKind::Definition ||
           kind == RelationKind::Reference) {
            assert(begin == end && "Expect a single location");

            /// If the location is a file location or its spelling location is in macro arguments,
            /// Then we expect to trigger goto on them to get the correct location.
            /// So add Occurrence for them.
            if(begin.isFileID() || (begin.isMacroID() && srcMgr.isMacroArgExpansion(begin))) {
                auto spelling = srcMgr.getSpellingLoc(begin);
                addOccurrence(addLocation(spelling), symbolRef);
            }
        }

        /// For relation we always use expansion relation, so that you can check which macro
        /// expansion generates the relation.
        auto expansion =
            clang::SourceRange(srcMgr.getExpansionLoc(begin), srcMgr.getExpansionLoc(end));
        addRelation(symbolRef, kind, addLocation(expansion));
    }

private:
    /// Index cache.
    using SourceRange = std::pair<clang::SourceLocation, clang::SourceLocation>;
    llvm::DenseMap<clang::FileID, std::size_t> fileCache;
    llvm::DenseMap<const clang::Decl*, std::size_t> symbolCache;
    llvm::DenseMap<SourceRange, std::size_t> locationCache;

    /// Index result.
    memory::Index* index = nullptr;
};

void sortSymbols(memory::Index& index) {
    /// new(index) -> old(value)
    std::vector<uint32_t> new2old(index.symbols.size());
    std::ranges::iota(new2old, 0u);

    /// Sort the symbols by `Symbol::id`.
    std::ranges::sort(std::ranges::views::zip(index.symbols, new2old), {}, [](const auto& element) {
        return std::get<0>(element).id;
    });

    /// old(index) -> new(value)
    std::vector<uint32_t> old2new(index.symbols.size());
    for(uint32_t i = 0; i < index.symbols.size(); ++i) {
        old2new[new2old[i]] = i;
    }

    /// Adjust the all symbol references.
    for(auto& occurrence: index.occurrences) {
        occurrence.symbol.offset = old2new[occurrence.symbol.offset];
    }

    /// FIXME: may need to adjust the relations.
    // for(auto& symbol : index.symbols) {
    //     for(auto& relation : symbol.relations) {
    //
    //     }
    // }
}

}  // namespace

/// Index the AST information.
memory::Index index(ASTInfo& info) {
    memory::Index index = {};
    IndexBuilder builder(info, index);
    builder.TraverseAST(info.context());

    /// Sort symbols by `Symbol::id`.
    sortSymbols(index);

    /// Sort occurrences by `Occurrence::Location`. Note that file is the first field of `Location`,
    /// this means that location with the same file will be adjacent.
    std::ranges::sort(index.occurrences, refl::less, [&](const Occurrence& occurrence) {
        return index.locations[occurrence.location.offset];
    });

    return index;
}

}  // namespace clice::index

namespace clice {

/// Convert `memory::Index` to JSON.
json::Value index::toJSON(const index::memory::Index& index) {
    return json::serialize(index);
}

}  // namespace clice
