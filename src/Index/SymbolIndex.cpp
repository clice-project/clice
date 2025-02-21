#include <numeric>

#include "Index/Index.h"
#include "Index/USR.h"
#include "AST/Semantic.h"
#include "Basic/SourceCode.h"
#include "Index/SymbolIndex.h"
#include "Support/Binary.h"
#include "Support/Compare.h"

namespace clice::index {

namespace {

struct SymbolIndexStorage : memory::SymbolIndex {
    std::uint32_t getLocation(LocalSourceRange range) {
        auto key = std::pair(range.begin, range.end);
        auto [iter, success] = locationCache.try_emplace(key, ranges.size());
        if(success) {
            ranges.emplace_back(range);
        }
        return iter->second;
    }

    std::uint32_t getSymbol(const void* symbol, uint64_t id, std::string name, SymbolKind kind) {
        auto [iter, success] = symbolCache.try_emplace(symbol, symbols.size());
        if(success) {
            symbols.emplace_back(memory::Symbol{
                .id = id,
                .name = std::move(name),
                .kind = kind,
            });
        }
        return iter->second;
    }

    void addOccurrence(uint32_t location, uint32_t symbol) {
        occurrences.emplace_back(memory::Occurrence{location, symbol});
    }

    void sort() {
        /// We will serialize the index to binary format and compare the data to
        /// check whether they are the index. So here we need to sort all vectors
        /// to make sure that the data is in the same order even they are in different
        /// files.

        /// Polyfill ranges::iota for libc++
        auto iota = [](auto& r, auto init) {
            ranges::generate(r, [init] mutable { return init++; });
        };

        /// Map the old index to new index.
        std::vector<uint32_t> symbolMap(symbols.size());
        std::vector<uint32_t> locationMap(ranges.size());

        {
            /// Sort symbols and update the symbolMap.
            std::vector<uint32_t> new2old(symbols.size());
            iota(new2old, 0u);

            ranges::sort(views::zip(symbols, new2old), refl::less, [](const auto& element) {
                auto& symbol = std::get<0>(element);
                return std::tuple(symbol.id, symbol.name, symbol.kind);
            });

            for(uint32_t i = 0; i < symbols.size(); ++i) {
                symbolMap[new2old[i]] = i;
            }
        }

        {
            /// Sort locations and update the locationMap.
            std::vector<uint32_t> new2old(ranges.size());
            iota(new2old, 0u);

            ranges::sort(views::zip(ranges, new2old), refl::less, [](const auto& element) {
                return std::get<0>(element);
            });

            for(uint32_t i = 0; i < ranges.size(); ++i) {
                locationMap[new2old[i]] = i;
            }
        }

        /// Sort occurrences and update the symbol and location references.
        for(auto& occurrence: occurrences) {
            occurrence.symbol = {symbolMap[occurrence.symbol]};
            occurrence.location = {locationMap[occurrence.location]};
        }

        /// Sort all occurrences and update the symbol and location references.
        ranges::sort(occurrences, refl::less, [](const auto& occurrence) {
            return occurrence.location;
        });
        auto range = ranges::unique(occurrences, refl::equal);
        occurrences.erase(range.begin(), range.end());

        /// Sort all relations and update the symbol and location references.
        for(auto& symbol: symbols) {
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

    llvm::DenseMap<const void*, uint32_t> symbolCache;
    llvm::DenseMap<std::pair<uint32_t, uint32_t>, uint32_t> locationCache;
};

class SymbolIndexCollector : public SemanticVisitor<SymbolIndexCollector> {
public:
    SymbolIndexCollector(ASTInfo& info) : SemanticVisitor(info, false) {}

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
            index::generateUSRForMacro(name, def->getDefinitionLoc(), SM, USR);
        } else {
            index::generateUSRForDecl(static_cast<const clang::Decl*>(symbol), USR);
        }

        assert(!USR.empty() && "Invalid USR");
        auto id = llvm::xxh3_64bits(USR);
        symbolIDs.try_emplace(symbol, id);
        return id;
    }

    std::uint32_t getSymbol(SymbolIndexStorage& file, const clang::NamedDecl* decl) {
        auto symbol = file.getSymbol(decl,
                                     getSymbolID(decl),
                                     decl->getNameAsString(),
                                     SymbolKind::from(decl));
        return symbol;
    }

public:
    void handleDeclOccurrence(const clang::NamedDecl* decl,
                              RelationKind kind,
                              clang::SourceLocation location) {
        assert(decl && "Invalid decl");
        decl = normalize(decl);

        if(location.isMacroID()) {
            auto spelling = AST.getSpellingLoc(location);
            auto expansion = AST.getExpansionLoc(location);

            /// FIXME: For location from macro, we only handle the case that the
            /// spelling and expansion are in the same file currently.
            if(AST.getFileID(spelling) != AST.getFileID(expansion)) {
                return;
            }

            /// For occurrence, we always use spelling location.
            location = spelling;
        }

        /// Add the occurrence.
        auto [fid, local] = AST.toLocalRange(location);
        auto& index = indices[fid];
        auto loc = index.getLocation(local);
        auto symbol = index.getSymbol(decl,
                                      getSymbolID(decl),
                                      decl->getNameAsString(),
                                      SymbolKind::from(decl));
        index.addOccurrence(loc, symbol);
    }

    void handleMacroOccurrence(const clang::MacroInfo* def,
                               RelationKind kind,
                               clang::SourceLocation location) {
        /// FIXME: Figure out when location is MacroID.
        if(location.isMacroID()) {
            return;
        }

        auto begin = def->getDefinitionLoc();
        auto end = def->getDefinitionEndLoc();
        assert(begin.isFileID() && end.isFileID() && "Invalid location");

        /// Get the macro name.
        auto name = getTokenSpelling(SM, begin);

        /// Add the occurrence.
        auto [fid, local] = AST.toLocalRange(location);
        auto& file = indices[fid];
        auto loc = file.getLocation(local);
        auto symbol = file.getSymbol(def, getSymbolID(def, true), name.str(), SymbolKind::Macro);
        file.addOccurrence(loc, symbol);

        /// If the macro is a definition, set definition range for it.
        memory::ValueRef data1 = {};
        if(kind & RelationKind::Definition) {
            auto [fid2, range] = AST.toLocalRange(clang::SourceRange(begin, end));
            assert(fid == fid2 && "Invalid macro definition location");
            data1.offset = file.getLocation(range);
        }

        auto& relations = file.symbols[symbol].relations;
        relations.emplace_back(memory::Relation{
            .kind = kind,
            .data = {loc},
            .data1 = data1,
        });
    }

    void handleRelation(const clang::NamedDecl* decl,
                        RelationKind kind,
                        const clang::NamedDecl* target,
                        clang::SourceRange range) {
        auto [begin, end] = range;

        /// For relation, we always use expansion location.
        begin = AST.getExpansionLoc(begin);
        end = AST.getExpansionLoc(end);
        assert(begin.isFileID() && end.isFileID() && "Invalid location");

        auto [fid, relationRange] = AST.toLocalRange(clang::SourceRange(begin, end));
        auto& file = indices[fid];

        /// Calculate the data for the relation.
        memory::ValueRef data[2] = {};
        using enum RelationKind::Kind;

        if(kind.is_one_of(Definition, Declaration)) {
            auto [begin, end] = decl->getSourceRange();
            begin = AST.getExpansionLoc(begin);
            end = AST.getExpansionLoc(end);
            auto [fid2, definitionRange] = AST.toLocalRange(clang::SourceRange(begin, end));
            assert(fid == fid2 && "Invalid definition location");

            data[0].offset = file.getLocation(relationRange);
            data[1].offset = file.getLocation(definitionRange);
        } else if(kind.is_one_of(Reference, WeakReference)) {
            data[0].offset = file.getLocation(relationRange);
        } else if(kind.is_one_of(Interface,
                                 Implementation,
                                 TypeDefinition,
                                 Base,
                                 Derived,
                                 Constructor,
                                 Destructor)) {
            data[0].offset = getSymbol(file, normalize(target));
        } else if(kind.is_one_of(Caller, Callee)) {
            data[0].offset = getSymbol(file, normalize(target));
            data[1].offset = file.getLocation(relationRange);
        } else {
            std::unreachable();
        }

        /// Add the relation.
        auto symbol = getSymbol(file, normalize(decl));
        file.symbols[symbol].relations.emplace_back(memory::Relation{
            .kind = kind,
            .data = data[0],
            .data1 = data[1],
        });
    }

    llvm::DenseMap<clang::FileID, SymbolIndex> build() {
        run();

        llvm::DenseMap<clang::FileID, SymbolIndex> result;
        for(auto& [fid, index]: indices) {
            index.sort();

            if(index.path.empty()) {
                index.path = AST.getFilePath(fid);
            }

            auto [buffer, size] = clice::binary::binarify(static_cast<memory::SymbolIndex>(index));
            result.try_emplace(
                fid,
                SymbolIndex{static_cast<char*>(const_cast<void*>(buffer.base)), size, true});
        }

        return std::move(result);
    }

private:
    llvm::DenseMap<const void*, uint64_t> symbolIDs;
    llvm::DenseMap<clang::FileID, SymbolIndexStorage> indices;
};

}  // namespace

Shared<SymbolIndex> index(ASTInfo& info) {
    SymbolIndexCollector collector(info);
    return collector.build();
}

}  // namespace clice::index
