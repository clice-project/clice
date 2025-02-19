#include <numeric>

#include "Index/Index.h"

#include "AST/Semantic.h"
#include "Basic/SourceCode.h"
#include "Index/SymbolIndex.h"
#include "Support/Binary.h"
#include "Support/Compare.h"
#include "clang/Index/USRGeneration.h"

namespace clice::index {

namespace {

const static clang::NamedDecl* normalize(const clang::NamedDecl* decl) {
    if(!decl) {
        std::abort();
    }

    decl = llvm::cast<clang::NamedDecl>(decl->getCanonicalDecl());

    if(auto ND = instantiatedFrom(llvm::cast<clang::NamedDecl>(decl))) {
        return llvm::cast<clang::NamedDecl>(ND->getCanonicalDecl());
    }

    return decl;
}

class SymbolIndexBuilder : public SemanticVisitor<SymbolIndexBuilder> {
public:
    SymbolIndexBuilder(ASTInfo& info) : SemanticVisitor(info, false) {}

    struct File : memory::SymbolIndex {
        llvm::DenseMap<const void*, uint32_t> symbolCache;
        llvm::DenseMap<std::pair<uint32_t, uint32_t>, uint32_t> locationCache;
    };

    /// Get the symbol id for the given decl.
    uint64_t getSymbolID(const void* symbol, bool isMacro = false) {
        auto iter = symbolIDs.find(symbol);
        if(iter != symbolIDs.end()) {
            return iter->second;
        }

        llvm::SmallString<128> USR;
        if(isMacro) {
            auto def = static_cast<const clang::MacroInfo*>(symbol);
            auto name = getTokenSpelling(SM, def->getDefinitionLoc());
            clang::index::generateUSRForMacro(name, def->getDefinitionLoc(), SM, USR);
        } else {
            clang::index::generateUSRForDecl(static_cast<const clang::Decl*>(symbol), USR);
        }

        assert(!USR.empty() && "Invalid USR");
        auto id = llvm::xxh3_64bits(USR);
        symbolIDs.try_emplace(symbol, id);
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

    auto getSymbol(File& file, const clang::MacroInfo* def) {
        auto [iter, success] = file.symbolCache.try_emplace(def, file.symbols.size());
        /// If insert success, then we need to add a new symbol.
        if(success) {
            file.symbols.emplace_back(memory::Symbol{
                .id = getSymbolID(def, true),
                .name = getTokenSpelling(SM, def->getDefinitionLoc()).str(),
                .kind = SymbolKind::Macro,
            });
        }
        return iter->second;
    }

    auto getLocation(File& file, clang::SourceRange range) {
        /// add new location.
        auto [begin, end] = range;
        auto presumedBegin = SM.getDecomposedExpansionLoc(begin);
        auto presumedEnd = SM.getDecomposedExpansionLoc(end);
        ///
        auto beginOffset = presumedBegin.second;
        auto endOffset = presumedEnd.second + getTokenLength(AST.srcMgr(), end);

        auto [iter, success] =
            file.locationCache.try_emplace({beginOffset, endOffset}, file.ranges.size());
        if(success) {
            file.ranges.emplace_back(LocalSourceRange{beginOffset, endOffset});
        }
        return iter->second;
    }

    void sort(File& file) {
        /// We will serialize the index to binary format and compare the data to
        /// check whether they are the index. So here we need to sort all vectors
        /// to make sure that the data is in the same order even they are in different
        /// files.

        /// Map the old index to new index.
        std::vector<uint32_t> symbolMap(file.symbols.size());
        std::vector<uint32_t> locationMap(file.ranges.size());

        {
            /// Sort symbols and update the symbolMap.
            auto new2old = views::iota(0) | views::take(file.symbols.size()) | ranges::to<std::vector<uint32_t>>();

            ranges::sort(views::zip(file.symbols, new2old), refl::less, [](const auto& element) {
                auto& symbol = std::get<0>(element);
                return std::tuple(symbol.id, symbol.name, symbol.kind);
            });

            for(uint32_t i = 0; i < file.symbols.size(); ++i) {
                symbolMap[new2old[i]] = i;
            }
        }

        {
            /// Sort locations and update the locationMap.
            auto new2old = views::iota(0) | views::take(file.ranges.size()) | ranges::to<std::vector<uint32_t>>();

            ranges::sort(views::zip(file.ranges, new2old), refl::less, [](const auto& element) {
                return std::get<0>(element);
            });

            for(uint32_t i = 0; i < file.ranges.size(); ++i) {
                locationMap[new2old[i]] = i;
            }
        }

        /// Sort occurrences and update the symbol and location references.
        for(auto& occurrence: file.occurrences) {
            occurrence.symbol = {symbolMap[occurrence.symbol]};
            occurrence.location = {locationMap[occurrence.location]};
        }

        ranges::sort(file.occurrences, refl::less, [](const auto& occurrence) {
            return occurrence.location;
        });

        auto range = ranges::unique(file.occurrences, refl::equal);
        file.occurrences.erase(range.begin(), range.end());

        /// Sort all relations and update the symbol and location references.
        for(auto& symbol: file.symbols) {
            for(auto& relation: symbol.relations) {
                auto kind = relation.kind;
                if(kind.is_one_of(RelationKind::Definition, RelationKind::Declaration)) {
                    relation.data = {locationMap[relation.data]};
                    relation.data1 = {locationMap[relation.data1]};
                } else if(kind.is_one_of(RelationKind::Reference, RelationKind::WeakReference)) {
                    relation.data = {locationMap[relation.data]};
                } else if(kind.is_one_of(RelationKind::Interface,
                                         RelationKind::Implementation,
                                         RelationKind::TypeDefinition,
                                         RelationKind::Base,
                                         RelationKind::Derived,
                                         RelationKind::Constructor,
                                         RelationKind::Destructor)) {
                    relation.data = {symbolMap[relation.data]};
                } else if(kind.is_one_of(RelationKind::Caller, RelationKind::Callee)) {
                    relation.data = {symbolMap[relation.data]};
                    relation.data1 = {locationMap[relation.data1]};
                } else {
                    assert(false && "Invalid relation kind");
                }
            }

            ranges::sort(symbol.relations, refl::less);

            auto range = ranges::unique(symbol.relations, refl::equal);
            symbol.relations.erase(range.begin(), range.end());
        }
    }

public:
    void handleDeclOccurrence(const clang::NamedDecl* decl,
                              RelationKind kind,
                              clang::SourceLocation location) {
        /// If the name is not available, then we will skip it.
        if(auto II = decl->getIdentifier(); !II || II->getName().empty()) {
            return;
        }

        decl = normalize(decl);

        /// We always use spelling location for occurrence.
        auto spelling = SM.getSpellingLoc(location);
        clang::FileID id = SM.getFileID(spelling);
        assert(id.isValid() && "Invalid file id");
        auto& file = files[id];

        auto symbol = getSymbol(file, decl);
        auto loc = getLocation(file, {spelling, spelling});
        file.occurrences.emplace_back(memory::Occurrence{loc, symbol});
    }

    void handleMacroOccurrence(const clang::MacroInfo* def,
                               RelationKind kind,
                               clang::SourceLocation location) {
        auto spelling = SM.getSpellingLoc(location);
        clang::FileID id = SM.getFileID(spelling);
        assert(id.isValid() && "Invalid file id");
        auto& file = files[id];

        auto symbol = getSymbol(file, def);
        auto loc = getLocation(file, {spelling, spelling});
        file.occurrences.emplace_back(memory::Occurrence{loc, symbol});

        {
            auto expansion = SM.getExpansionLoc(location);
            clang::FileID id = SM.getFileID(expansion);
            assert(id.isValid() && "Invalid file id");

            auto& file = files[id];
            auto loc = getLocation(file, {expansion, expansion});
            auto symbol = getSymbol(file, def);

            if(kind & RelationKind::Definition) {
                file.symbols[symbol].relations.emplace_back(memory::Relation{
                    .kind = kind,
                    .data = {loc},
                    .data1 = {getLocation(
                        file,
                        clang::SourceRange(def->getDefinitionLoc(), def->getDefinitionEndLoc()))},
                });
            } else {
                file.symbols[symbol].relations.emplace_back(memory::Relation{
                    .kind = kind,
                    .data = {loc},
                });
            }
        }
    }

    void handleRelation(const clang::NamedDecl* decl,
                        RelationKind kind,
                        const clang::NamedDecl* target,
                        clang::SourceRange range) {
        /// FIXME: We should use a better way to handle anonymous decl.
        /// If the name is not available, then we will skip it.
        if(auto II = decl->getIdentifier(); !II || II->getName().empty()) {
            return;
        }

        const clang::NamedDecl* original = decl;
        bool sameDecl = decl == target;
        decl = normalize(decl);
        target = sameDecl ? decl : normalize(target);

        auto [begin, end] = range;
        auto expansion = SM.getExpansionLoc(begin);
        if(SM.isWrittenInBuiltinFile(expansion) ||
           SM.isWrittenInCommandLineFile(expansion)) {
            return;
        }

        assert(expansion.isValid() && expansion.isFileID() && "Invalid expansion location");
        clang::FileID id = SM.getFileID(expansion);
        assert(id.isValid() && "Invalid file id");
        assert(id == SM.getFileID(SM.getExpansionLoc(end)) && "Source range cross file");

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
        for(auto& [fid, file]: files) {
            auto loc = SM.getLocForStartOfFile(fid);

            /// FIXME: Figure out why index result will contain them.
            if(SM.isWrittenInBuiltinFile(loc) || SM.isWrittenInCommandLineFile(loc) ||
               SM.isWrittenInScratchSpace(loc)) {
                continue;
            }

            if(file.path.empty()) {
                llvm::SmallString<128> path;
                auto error =
                    llvm::sys::fs::real_path(SM.getFileEntryRefForID(fid)->getName(), path);
                if(!error) {
                    file.path = path.str();
                } else {
                    path = SM.getFileEntryRefForID(fid)->getName();
                }
            }

            auto [buffer, size] = clice::binary::binarify(static_cast<memory::SymbolIndex>(file));
            indices.try_emplace(
                fid,
                SymbolIndex{static_cast<char*>(const_cast<void*>(buffer.base)), size, true});
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
