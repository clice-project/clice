#include "BinarySymbolIndex.h"
#include "Compiler/Semantic.h"
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

class SymbolIndexCollector : public SemanticVisitor<SymbolIndexCollector> {
public:
    SymbolIndexCollector(ASTInfo& info) : SemanticVisitor(info) {}

    struct Symbol {
        uint64_t id;
        std::string name;
        SymbolKind kind;
        std::vector<binary::Relation> relations;
    };

    struct File {
        std::vector<Symbol> symbols;
        std::vector<binary::Occurrence> occurrences;
        std::vector<Location> locations;

        llvm::DenseMap<const void*, uint32_t> symbolCache;
        llvm::DenseMap<std::pair<clang::SourceLocation, clang::SourceLocation>, uint32_t>
            locationCache;
    };

    struct Encoder {
        uint32_t symbolBegin = 0;
        uint32_t occurrenceBegin = 0;
        uint32_t relationBegin = 0;
        uint32_t locationBegin = 0;
        uint32_t stringBegin = 0;
        char* buffer;
        File* file;

        binary::String writeString(llvm::StringRef string) {
            auto size = string.size();
            auto begin = stringBegin;
            std::memcpy(buffer + stringBegin, string.data(), size);
            buffer[stringBegin + size] = '\0';
            stringBegin += size + 1;
            return {begin, static_cast<uint32_t>(size)};
        }

        binary::Relation writeRelation(binary::Relation relation) {
            auto offset = relationBegin;
            std::memcpy(buffer + relationBegin, &relation, sizeof(relation));
            relationBegin += sizeof(relation);

            return relation;
        }

        binary::Occurrence writeOccurrence(binary::Occurrence occurrence) {
            auto offset = occurrenceBegin;
            std::memcpy(buffer + occurrenceBegin, &occurrence, sizeof(occurrence));
            occurrenceBegin += sizeof(occurrence);
            return occurrence;
        }

        void writeLocation(Location location) {
            auto offset = locationBegin;
            std::memcpy(buffer + locationBegin, &location, sizeof(location));
            locationBegin += sizeof(location);
        }

        binary::Symbol writeSymbol(Symbol symbol) {
            auto offset = symbolBegin;
            binary::Symbol binarySymbol = {
                .id = symbol.id,
                .name = writeString(symbol.name),
                .kind = symbol.kind,
                .relations = {relationBegin, static_cast<uint32_t>(symbol.relations.size())},
            };

            for(auto& relation: symbol.relations) {
                writeRelation(relation);
            }

            std::memcpy(buffer + offset, &binarySymbol, sizeof(binarySymbol));
            symbolBegin += sizeof(binary::Symbol);
            return binarySymbol;
        }

        SymbolIndex writeIndex(File& file) {

            /// ========================================
            /// |              SymbolIndex             |
            /// ========================================
            /// |                Symbols               |
            /// ========================================
            /// |              Occurrences             |
            /// ========================================
            /// |               Relations              |
            /// ========================================
            /// |               Locations              |
            /// ========================================
            /// |                Strings               |
            /// ========================================

            symbolBegin = sizeof(binary::SymbolIndex);

            occurrenceBegin = symbolBegin + file.symbols.size() * sizeof(binary::Symbol);

            relationBegin = occurrenceBegin + file.occurrences.size() * sizeof(binary::Occurrence);

            locationBegin = relationBegin;
            for(auto& symbol: file.symbols) {
                locationBegin += symbol.relations.size() * sizeof(binary::Relation);
            }

            stringBegin = locationBegin + file.locations.size() * sizeof(Location);

            std::size_t size = stringBegin;
            for(auto& symbol: file.symbols) {
                size += symbol.name.size() + 1;
            }

            buffer = static_cast<char*>(std::malloc(size));
            this->file = &file;
            auto index = new (buffer) binary::SymbolIndex{};

            index->symbols = {symbolBegin, static_cast<uint32_t>(file.symbols.size())};
            index->occurrences = {occurrenceBegin, static_cast<uint32_t>(file.occurrences.size())};
            index->locations = {locationBegin, static_cast<uint32_t>(file.locations.size())};

            for(auto& symbol: file.symbols) {
                writeSymbol(symbol);
            }

            for(auto& occurrence: file.occurrences) {
                writeOccurrence(occurrence);
            }

            for(auto& location: file.locations) {
                writeLocation(location);
            }

            return SymbolIndex(buffer, size);
        }
    };

public:
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
            file.symbols.emplace_back(Symbol{
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
        auto [iter, success] = file.locationCache.try_emplace({begin, end}, file.locations.size());
        if(success) {
            auto presumedBegin = srcMgr.getPresumedLoc(begin);
            auto presumedEnd = srcMgr.getPresumedLoc(end);
            auto length = info.getTokenLength(end);
            file.locations.emplace_back(Location{
                .begin = {presumedBegin.getLine(), presumedBegin.getColumn()       },
                .end = {presumedEnd.getLine(),   presumedEnd.getColumn() + length},
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
            occurrence.symbol = old2new[occurrence.symbol];
        }

        /// FIXME: may need to adjust the relations.

        /// Sort occurrences by `Occurrence::Location`. Note that file is the first field of
        /// `Location`, this means that location with the same file will be adjacent.
        std::ranges::sort(file.occurrences, refl::less, [&](const auto& occurrence) {
            return file.locations[occurrence.location];
        });
    }

public:
    void handleDeclOccurrence(const clang::NamedDecl* decl,
                              clang::SourceLocation location,
                              RelationKind kind) {
        decl = normalize(decl);

        /// We always use spelling location for occurrence.
        auto spelling = srcMgr.getSpellingLoc(location);
        clang::FileID id = srcMgr.getFileID(spelling);
        assert(id.isValid() && "Invalid file id");
        auto& file = files[id];

        auto symbol = getSymbol(file, decl);
        auto loc = getLocation(file, {spelling, spelling});
        file.occurrences.emplace_back(binary::Occurrence{loc, symbol});
    }

    void handleMacroOccurrence(const clang::MacroInfo* def,
                               clang::SourceLocation location,
                               RelationKind kind) {
        auto spelling = srcMgr.getSpellingLoc(location);
        clang::FileID id = srcMgr.getFileID(spelling);
        assert(id.isValid() && "Invalid file id");
        auto& file = files[id];
    }

    void handleRelation(const clang::NamedDecl* decl,
                        RelationKind kind,
                        const clang::NamedDecl* target,
                        clang::SourceRange range) {
        bool sameDecl = decl == target;
        decl = normalize(decl);
        target = sameDecl ? decl : normalize(target);

        auto [begin, end] = range;
        auto expansion = srcMgr.getExpansionLoc(begin);
        assert(expansion.isValid() && expansion.isFileID() && "Invalid expansion location");
        clang::FileID id = srcMgr.getFileID(expansion);
        assert(id.isValid() && "Invalid file id");
        assert(id == srcMgr.getFileID(srcMgr.getExpansionLoc(end)) && "Source range cross file");

        auto& file = files[id];
        auto symbol = getSymbol(file, decl);
        auto extra = sameDecl ? symbol : getSymbol(file, target);

        file.symbols[symbol].relations.emplace_back(binary::Relation{
            .kind = kind,
            .location = getLocation(file, range),
            .extra = extra,
        });
    }

    llvm::DenseMap<clang::FileID, SymbolIndex> build() {
        run();

        for(auto& [_, file]: files) {
            sort(file);
        }

        llvm::DenseMap<clang::FileID, SymbolIndex> indices;
        for(auto& [id, file]: files) {
            indices.try_emplace(id, Encoder().writeIndex(file));
        }
        return std::move(indices);
    }

private:
    llvm::DenseMap<const void*, uint64_t> symbolIDs;
    llvm::DenseMap<clang::FileID, File> files;
};

}  // namespace

llvm::DenseMap<clang::FileID, SymbolIndex> test(ASTInfo& info) {
    SymbolIndexCollector collector(info);
    return collector.build();
}

}  // namespace clice::index
