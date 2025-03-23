#include <numeric>

#include "AST/Semantic.h"
#include "AST/SourceCode.h"
#include "Index/SymbolIndex.h"
#include "Support/Binary.h"
#include "Support/Compare.h"

namespace clice::index {

namespace {

namespace memory {

struct Relation {
    RelationKind kind;

    /// The `data` array contains two fields whose meanings depend on the `kind`.
    /// Each `RelationKind` specifies the interpretation of these fields as follows:
    ///
    /// - `Definition` and `Declaration`:
    ///   - `data[0]`: The range of the name token.
    ///   - `data[1]`: The range of the whole symbol.
    ///
    /// - `Reference` and `WeakReference`:
    ///   - `data[0]`: The range of the reference.
    ///   - `data[1]`: Empty (unused).
    ///
    /// - `Interface`, `Implementation`, `TypeDefinition`, `Base`, `Derived`,
    ///   `Constructor`, and `Destructor`:
    ///   - `data[0]`: The target symbol.
    ///   - `data[1]`: Empty (unused).
    ///
    /// - `Caller` and `Callee`:
    ///   - `data[0]`: The target symbol (e.g., the called function).
    ///   - `data[1]`: The range of the call site.
    ///
    std::uint32_t data = -1;
    std::uint32_t data1 = -1;
};

struct Symbol {
    /// The symbol id.
    SymbolID id;

    /// The symbol kind.
    SymbolKind kind;

    /// The relations of this symbol.
    std::vector<Relation> relations;
};

struct Occurrence {
    /// The location(index) of this symbol occurrence.
    std::uint32_t location = -1;

    /// The referenced symbol(index) of the this symbol occurrence.
    std::uint32_t symbol = -1;
};

struct SymbolIndex {
    /// The path of source file.
    std::string path;

    /// The content of source file.
    std::string content;

    /// FIXME: add includes or module names?

    /// All symbols in this file.
    std::vector<Symbol> symbols;

    /// All occurrences in this file.
    std::vector<Occurrence> occurrences;

    /// All ranges in this file.
    std::vector<LocalSourceRange> ranges;
};

}  // namespace memory

class SymbolIndexBuilder : public memory::SymbolIndex {
public:
    SymbolIndexBuilder(ASTInfo& AST) : AST(AST) {}

    std::uint32_t getLocation(LocalSourceRange range) {
        auto key = std::pair(range.begin, range.end);
        auto [iter, success] = locationCache.try_emplace(key, ranges.size());
        if(success) {
            ranges.emplace_back(range);
        }
        return iter->second;
    }

    std::uint32_t getSymbol(const clang::NamedDecl* decl) {
        auto [iter, success] = symbolCache.try_emplace(decl, symbols.size());
        if(success) {
            symbols.emplace_back(memory::Symbol{
                .id = AST.getSymbolID(decl),
                .kind = SymbolKind::from(decl),
            });
        }
        return iter->second;
    }

    std::uint32_t getSymbol(const clang::MacroInfo* macro) {
        auto [iter, success] = symbolCache.try_emplace(macro, symbols.size());
        if(success) {
            symbols.emplace_back(memory::Symbol{
                .id = AST.getSymbolID(macro),
                .kind = SymbolKind::Macro,
            });
        }
        return iter->second;
    }

    void addOccurrence(uint32_t location, uint32_t symbol) {
        occurrences.emplace_back(memory::Occurrence{
            .location = location,
            .symbol = symbol,
        });
    }

    void addRelation(uint32_t symbol,
                     RelationKind kind,
                     uint32_t data,
                     uint32_t data1 = std::numeric_limits<uint32_t>::max()) {
        symbols[symbol].relations.emplace_back(memory::Relation{
            .kind = kind,
            .data = data,
            .data1 = data1,
        });
    }

    void sort() {
        /// We will serialize the index to binary format and compare the data to
        /// check whether they are the index. So here we need to sort all vectors
        /// to make sure that the data is in the same order even they are in different
        /// files.

        /// Map the old index to new index.
        std::vector<uint32_t> symbolMap(symbols.size());
        std::vector<uint32_t> locationMap(ranges.size());

        {
            /// Sort symbols and update the symbolMap.
            std::vector<uint32_t> new2old(symbols.size());
            for(uint32_t i = 0; i < symbols.size(); ++i) {
                new2old[i] = i;
            }

            ranges::sort(views::zip(symbols, new2old), refl::less, [](const auto& element) {
                auto& symbol = std::get<0>(element);
                return std::tuple(symbol.id, symbol.kind);
            });

            for(uint32_t i = 0; i < symbols.size(); ++i) {
                symbolMap[new2old[i]] = i;
            }
        }

        {
            /// Sort locations and update the locationMap.
            std::vector<uint32_t> new2old(ranges.size());
            for(uint32_t i = 0; i < ranges.size(); ++i) {
                new2old[i] = i;
            }

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

        using enum RelationKind::Kind;
        /// Sort all relations and update the symbol and location references.
        for(auto& symbol: symbols) {
            for(auto& relation: symbol.relations) {
                auto kind = relation.kind;
                if(kind.is_one_of(Definition, Declaration)) {
                    relation.data = locationMap[relation.data];
                    relation.data1 = locationMap[relation.data1];
                } else if(kind.is_one_of(Reference, WeakReference)) {
                    relation.data = locationMap[relation.data];
                } else if(kind.is_one_of(Interface,
                                         Implementation,
                                         TypeDefinition,
                                         Base,
                                         Derived,
                                         Constructor,
                                         Destructor)) {
                    relation.data = symbolMap[relation.data];
                } else if(kind.is_one_of(Caller, Callee)) {
                    relation.data = symbolMap[relation.data];
                    relation.data1 = locationMap[relation.data1];
                } else {
                    assert(false && "Invalid relation kind");
                }
            }

            ranges::sort(symbol.relations, refl::less);

            auto range = ranges::unique(symbol.relations, refl::equal);
            symbol.relations.erase(range.begin(), range.end());
        }
    }

    memory::SymbolIndex dump() {
        return std::move(static_cast<memory::SymbolIndex>(*this));
    }

private:
    ASTInfo& AST;
    llvm::DenseMap<const void*, uint32_t> symbolCache;
    llvm::DenseMap<std::pair<uint32_t, uint32_t>, uint32_t> locationCache;
};

class SymbolIndexCollector : public SemanticVisitor<SymbolIndexCollector> {
public:
    SymbolIndexCollector(ASTInfo& AST) : SemanticVisitor(AST, false) {}

    SymbolIndexBuilder& getBuilder(clang::FileID fid) {
        auto [it, success] = builders.try_emplace(fid, AST);
        return it->second;
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
        auto [fid, range] = AST.toLocalRange(location);
        auto& builder = getBuilder(fid);
        auto loc = builder.getLocation(range);
        auto symbol = builder.getSymbol(decl);
        builder.addOccurrence(loc, symbol);
    }

    void handleMacroOccurrence(const clang::MacroInfo* def,
                               RelationKind kind,
                               clang::SourceLocation location) {
        /// FIXME: Figure out when location is MacroID.
        if(location.isMacroID()) {
            return;
        }

        /// Add macro occurrence.
        auto [fid, range] = AST.toLocalRange(location);
        auto& builder = getBuilder(fid);
        auto loc = builder.getLocation(range);
        auto symbol = builder.getSymbol(def);
        builder.addOccurrence(loc, symbol);

        /// If the macro is a definition, set definition range for it.
        std::uint32_t definitionLoc = std::numeric_limits<std::uint32_t>::max();

        if(kind & RelationKind::Definition) {
            auto begin = def->getDefinitionLoc();
            auto end = def->getDefinitionEndLoc();
            assert(begin.isFileID() && end.isFileID() && "Invalid location");
            auto [fid2, range] = AST.toLocalRange(clang::SourceRange(begin, end));
            assert(fid == fid2 && "Invalid macro definition location");
            definitionLoc = builder.getLocation(range);
        }

        builder.addRelation(symbol, kind, loc, definitionLoc);
    }

    void handleRelation(const clang::NamedDecl* decl,
                        RelationKind kind,
                        const clang::NamedDecl* target,
                        clang::SourceRange range) {
        auto [fid, relationRange] = AST.toLocalExpansionRange(range);
        auto& builder = getBuilder(fid);

        /// Calculate the data for the relation.
        std::uint32_t data[2] = {
            std::numeric_limits<std::uint32_t>::max(),
            std::numeric_limits<std::uint32_t>::max(),
        };

        using enum RelationKind::Kind;

        if(kind.is_one_of(Definition, Declaration)) {
            auto [fid2, definitionRange] = AST.toLocalExpansionRange(decl->getSourceRange());
            assert(fid == fid2 && "Invalid definition location");
            data[0] = builder.getLocation(relationRange);
            data[1] = builder.getLocation(definitionRange);
        } else if(kind.is_one_of(Reference, WeakReference)) {
            data[0] = builder.getLocation(relationRange);
        } else if(kind.is_one_of(Interface,
                                 Implementation,
                                 TypeDefinition,
                                 Base,
                                 Derived,
                                 Constructor,
                                 Destructor)) {
            data[0] = builder.getSymbol(normalize(target));
        } else if(kind.is_one_of(Caller, Callee)) {
            data[0] = builder.getSymbol(normalize(target));
            data[1] = builder.getLocation(relationRange);
        } else {
            std::unreachable();
        }

        /// Add the relation.
        auto symbol = builder.getSymbol(normalize(decl));
        builder.addRelation(symbol, kind, data[0], data[1]);
    }

    auto build() {
        run();

        llvm::DenseMap<clang::FileID, std::vector<char>> result;
        for(auto& [fid, builder]: builders) {
            builder.sort();
            auto index = builder.dump();
            index.path = AST.getFilePath(fid);
            index.content = AST.getFileContent(fid);
            auto [buffer, _] = binary::serialize(index);
            result.try_emplace(fid, std::move(buffer));
        }

        return std::move(result);
    }

private:
    llvm::DenseMap<const void*, uint64_t> symbolIDs;
    llvm::DenseMap<clang::FileID, SymbolIndexBuilder> builders;
};

}  // namespace

Shared<std::vector<char>> index(ASTInfo& AST) {
    return SymbolIndexCollector(AST).build();
}

}  // namespace clice::index
