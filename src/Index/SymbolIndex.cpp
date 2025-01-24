#include <numeric>

#include "Index.h"

#include "Basic/SourceCode.h"
#include "Index/SymbolIndex.h"
#include "Compiler/Semantic.h"
#include "Support/Binary.h"

#include "clang/Index/USRGeneration.h"

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

class SymbolIndexBuilder : public SemanticVisitor<SymbolIndexBuilder> {
public:
    SymbolIndexBuilder(ASTInfo& info) : SemanticVisitor(info) {}

    struct File : memory::SymbolIndex {
        llvm::DenseMap<const void*, uint32_t> symbolCache;
        llvm::DenseMap<std::pair<clang::SourceLocation, clang::SourceLocation>, uint32_t>
            locationCache;
    };

    /// Get the symbol id for the given decl.
    uint64_t getSymbolID(const clang::Decl* decl) {
        auto iter = symbolIDs.find(decl);
        if(iter != symbolIDs.end()) {
            return iter->second;
        }

        llvm::SmallString<128> USR;
        clang::index::generateUSRForDecl(decl, USR);
        assert(!USR.empty() && "Invalid USR");
        auto id = llvm::xxh3_64bits(USR);
        symbolIDs.try_emplace(decl, id);
        return id;
    }

    auto getSymbol(File& file, const clang::NamedDecl* decl) {
        auto [iter, success] = file.symbolCache.try_emplace(decl, file.symbols.size());
        /// If insert success, then we need to add a new symbol.
        if(success) {
            file.symbols.emplace_back(memory::Symbol{
                .id = getSymbolID(decl),
                .name = decl->getNameAsString(),
                .kind = SymbolKind::from(decl),
            });
        }
        return iter->second;
    }

    /// TODO: handle macro reference.

    auto getLocation(File& file, clang::SourceRange range) {
        /// add new location.
        auto [begin, end] = range;
        auto [iter, success] = file.locationCache.try_emplace({begin, end}, file.ranges.size());
        if(success) {
            auto presumedBegin = srcMgr.getDecomposedExpansionLoc(begin);
            auto presumedEnd = srcMgr.getDecomposedExpansionLoc(end);
            ///
            file.ranges.emplace_back(LocalSourceRange{
                .begin = presumedBegin.second,
                .end = presumedEnd.second + getTokenLength(info.srcMgr(), end),
            });
        }
        return iter->second;
    }

    void sort(File& file) {
        /// new(index) -> old(value)
        std::vector<uint32_t> new2old(file.symbols.size());
        std::ranges::iota(new2old, 0u);

        /// Sort the symbols by `Symbol::id`.
        std::ranges::sort(std::ranges::views::zip(file.symbols, new2old),
                          {},
                          [](const auto& element) { return std::get<0>(element).id; });

        /// old(index) -> new(value)
        std::vector<uint32_t> old2new(file.symbols.size());
        for(uint32_t i = 0; i < file.symbols.size(); ++i) {
            old2new[new2old[i]] = i;
        }

        /// Adjust the all symbol references.
        for(auto& occurrence: file.occurrences) {
            occurrence.symbol = {old2new[occurrence.symbol]};
        }

        /// FIXME: may need to adjust the relations.

        /// Sort occurrences by `Occurrence::Location`. Note that file is the first field of
        /// `Location`, this means that location with the same file will be adjacent.
        std::ranges::sort(file.occurrences, refl::less, [&](const auto& occurrence) {
            return file.ranges[occurrence.location];
        });

        /// Remove duplicate occurrences.
        auto range = ranges::unique(file.occurrences, refl::equal);
        file.occurrences.erase(range.begin(), range.end());
    }

public:
    void handleDeclOccurrence(const clang::NamedDecl* decl,
                              RelationKind kind,
                              clang::SourceLocation location) {
        decl = normalize(decl);

        /// We always use spelling location for occurrence.
        auto spelling = srcMgr.getSpellingLoc(location);
        clang::FileID id = srcMgr.getFileID(spelling);
        assert(id.isValid() && "Invalid file id");
        auto& file = files[id];

        auto symbol = getSymbol(file, decl);
        auto loc = getLocation(file, {spelling, spelling});
        file.occurrences.emplace_back(memory::Occurrence{loc, symbol});
    }

    void handleMacroOccurrence(const clang::MacroInfo* def,
                               RelationKind kind,
                               clang::SourceLocation location) {
        /// auto spelling = srcMgr.getSpellingLoc(location);
        /// clang::FileID id = srcMgr.getFileID(spelling);
        /// assert(id.isValid() && "Invalid file id");
        /// auto& file = files[id];
        /// TODO:
    }

    void handleRelation(const clang::NamedDecl* decl,
                        RelationKind kind,
                        const clang::NamedDecl* target,
                        clang::SourceRange range) {
        const clang::NamedDecl* original = decl;
        bool sameDecl = decl == target;
        decl = normalize(decl);
        target = sameDecl ? decl : normalize(target);

        auto [begin, end] = range;
        auto expansion = srcMgr.getExpansionLoc(begin);
        if(srcMgr.isWrittenInBuiltinFile(expansion) ||
           srcMgr.isWrittenInCommandLineFile(expansion)) {
            return;
        }

        assert(expansion.isValid() && expansion.isFileID() && "Invalid expansion location");
        clang::FileID id = srcMgr.getFileID(expansion);
        assert(id.isValid() && "Invalid file id");
        assert(id == srcMgr.getFileID(srcMgr.getExpansionLoc(end)) && "Source range cross file");

        auto& file = files[id];

        memory::ValueRef data[2] = {};
        if(kind.is_one_of(RelationKind::Definition, RelationKind::Declaration)) {
            data[0] = {getLocation(file, range)};
            data[1] = {getLocation(file, original->getSourceRange())};
        } else if(kind.is_one_of(RelationKind::Reference, RelationKind::WeakReference)) {
            data[0] = {getLocation(file, range)};
        } else if(kind.is_one_of(RelationKind::Interface,
                                 RelationKind::Implementation,
                                 RelationKind::TypeDefinition,
                                 RelationKind::Base,
                                 RelationKind::Derived,
                                 RelationKind::Constructor,
                                 RelationKind::Destructor)) {
            data[0] = {getSymbol(file, target)};
        } else if(kind.is_one_of(RelationKind::Caller, RelationKind::Callee)) {
            data[0] = {getSymbol(file, target)};
            data[1] = {getLocation(file, range)};
        } else {
            assert(false && "Invalid relation kind");
        }

        auto symbol = getSymbol(file, decl);
        file.symbols[symbol].relations.emplace_back(memory::Relation{
            .kind = kind,
            .data = data[0],
            .data1 = data[1],
        });
    }

    llvm::DenseMap<clang::FileID, SymbolIndex> build() {
        run();

        for(auto& [_, file]: files) {
            sort(file);
        }

        llvm::DenseMap<clang::FileID, SymbolIndex> indices;
        for(auto& [id, file]: files) {
            auto [buffer, size] = clice::binary::binarify(static_cast<memory::SymbolIndex>(file));
            indices.try_emplace(id, SymbolIndex{const_cast<void*>(buffer.base), size, true});
        }
        return std::move(indices);
    }

private:
    llvm::DenseMap<const void*, uint64_t> symbolIDs;
    llvm::DenseMap<clang::FileID, File> files;
};

}  // namespace

Shared<SymbolIndex> index(ASTInfo& info) {
    SymbolIndexBuilder collector(info);
    return collector.build();
}

}  // namespace clice::index
