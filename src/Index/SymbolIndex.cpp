#include "Compiler/Semantic.h"
#include "Index/SymbolIndex.h"
#include "clang/Index/USRGeneration.h"

namespace clice::index {

namespace {

/// This namespace defines the binary format of the index file. Generally,
/// transform all pointer to offset to base address and cache location in the
/// location array. And data only will be deserialized when it is accessed.
namespace binary {

template <typename T>
struct Array {
    /// offset to index start.
    uint32_t offset;

    /// number of elements.
    uint32_t size;
};

using String = Array<char>;

struct Relation {
    RelationKind kind;
    uint32_t location = std::numeric_limits<uint32_t>::max();
    uint32_t extra = std::numeric_limits<uint32_t>::max();
};

struct Symbol {
    uint64_t id;
    String name;
    SymbolKind kind;
    Array<Relation> relations;
};

struct Occurrence {
    uint32_t location = std::numeric_limits<uint32_t>::max();
    uint32_t symbol = std::numeric_limits<uint32_t>::max();
};

struct SymbolIndex {
    Array<Symbol> symbols;
    Array<Occurrence> occurrences;
    Array<Location> locations;
};

struct ProxyIndex : SymbolIndex {
    llvm::StringRef getString(String string) const {
        return {reinterpret_cast<const char*>(this) + string.offset, string.size};
    }

    template <typename T>
    llvm::ArrayRef<T> getArray(Array<T> array) const {
        return {reinterpret_cast<const T*>(reinterpret_cast<const char*>(this) + array.offset),
                array.size};
    }

    llvm::ArrayRef<Symbol> getSymbols() const {
        return getArray(symbols);
    }

    llvm::ArrayRef<Occurrence> getOccurrences() const {
        return getArray(occurrences);
    }

    llvm::ArrayRef<Location> getLocations() const {
        return getArray(locations);
    }

    template <typename To, typename From>
    ArrayView<To> getArrayView(Array<From> array) const {
        auto base = reinterpret_cast<const char*>(this);
        return {base, base + array.offset, array.size, sizeof(From)};
    }
};

}  // namespace binary

}  // namespace

RelationKind SymbolIndex::Relation::kind() {
    return static_cast<const binary::Relation*>(data)->kind;
}

Location SymbolIndex::Relation::range() {
    auto index = static_cast<const binary::ProxyIndex*>(base);
    auto relation = static_cast<const binary::Relation*>(data);
    assert(relation->location != std::numeric_limits<uint32_t>::max() &&
           "Invalid location reference");
    return index->getLocations()[relation->location];
}

SymbolIndex::Symbol SymbolIndex::Relation::symbol() {
    auto index = static_cast<const binary::ProxyIndex*>(base);
    auto relation = static_cast<const binary::Relation*>(data);
    assert(relation->extra != std::numeric_limits<uint32_t>::max() && "Invalid extra reference");
    return {base, &index->getSymbols()[relation->extra]};
}

uint64_t SymbolIndex::SymbolID::id() {
    return static_cast<const binary::Symbol*>(data)->id;
}

llvm::StringRef SymbolIndex::SymbolID::name() {
    auto index = static_cast<const binary::ProxyIndex*>(base);
    auto symbol = static_cast<const binary::Symbol*>(data);
    return index->getString(symbol->name);
}

SymbolKind SymbolIndex::Symbol::kind() {
    return static_cast<const binary::Symbol*>(data)->kind;
}

ArrayView<SymbolIndex::Relation> SymbolIndex::Symbol::relations() {
    auto index = static_cast<const binary::ProxyIndex*>(base);
    auto symbol = static_cast<const binary::Symbol*>(data);
    return index->getArrayView<SymbolIndex::Relation>(symbol->relations);
}

Location SymbolIndex::Occurrence::location() {
    auto index = static_cast<const binary::ProxyIndex*>(base);
    auto occurrence = static_cast<const binary::Occurrence*>(data);
    assert(occurrence->location != std::numeric_limits<uint32_t>::max() &&
           "Invalid occurrence reference");
    return index->getLocations()[occurrence->location];
}

SymbolIndex::Symbol SymbolIndex::Occurrence::symbol() {
    auto index = static_cast<const binary::ProxyIndex*>(base);
    auto occurrence = static_cast<const binary::Occurrence*>(data);
    assert(occurrence->symbol != std::numeric_limits<uint32_t>::max() &&
           "Invalid symbol reference");
    return {base, &index->getSymbols()[occurrence->symbol]};
}

ArrayView<SymbolIndex::Symbol> SymbolIndex::symbols() {
    auto index = static_cast<const binary::ProxyIndex*>(base);
    return index->getArrayView<SymbolIndex::Symbol>(index->symbols);
}

ArrayView<SymbolIndex::Occurrence> SymbolIndex::occurrences() {
    auto index = static_cast<const binary::ProxyIndex*>(base);
    return index->getArrayView<SymbolIndex::Occurrence>(index->occurrences);
}

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

        llvm::DenseMap<const clang::Decl*, uint32_t> symbolCache;
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

        Location writeLocation(Location location) {
            auto offset = locationBegin;
            std::memcpy(buffer + locationBegin, &location, sizeof(location));
            locationBegin += sizeof(location);
            return location;
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
            symbolBegin = sizeof(binary::SymbolIndex);
            occurrenceBegin = symbolBegin + file.symbols.size() * sizeof(binary::Symbol);
            relationBegin = occurrenceBegin + file.occurrences.size() * sizeof(binary::Occurrence);
            for(auto& symbol: file.symbols) {
                relationBegin += symbol.relations.size() * sizeof(binary::Relation);
            }
            locationBegin = relationBegin + file.occurrences.size() * sizeof(binary::Relation);
            stringBegin = locationBegin + file.locations.size() * sizeof(Location);
            std::size_t size = stringBegin;
            for(auto& symbol: file.symbols) {
                size += symbol.name.size() + 1;
            }

            buffer = static_cast<char*>(std::malloc(size));
            auto index = new (buffer) binary::SymbolIndex{};

            index->symbols = {symbolBegin, static_cast<uint32_t>(file.symbols.size())};
            index->occurrences = {occurrenceBegin, static_cast<uint32_t>(file.occurrences.size())};
            index->locations = {locationBegin, static_cast<uint32_t>(file.locations.size())};

            for(auto& symbol: file.symbols) {
                writeSymbol(symbol);
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

    auto getLocation(File& file, clang::SourceRange range) {
        /// add new location.
        auto [begin, end] = range;
        auto [iter, success] = file.locationCache.try_emplace({begin, end}, file.locations.size());
        if(success) {
            auto presumedBegin = srcMgr.getPresumedLoc(begin);
            auto presumedEnd = srcMgr.getPresumedLoc(end);
            auto length = info.getTokenLength(end);
            file.locations.push_back(Location{
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
    llvm::DenseMap<const clang::Decl*, uint64_t> symbolIDs;
    llvm::DenseMap<clang::FileID, File> files;
};

}  // namespace

llvm::DenseMap<clang::FileID, SymbolIndex> test(ASTInfo& info) {
    SymbolIndexCollector collector(info);
    return collector.build();
}

}  // namespace clice::index
