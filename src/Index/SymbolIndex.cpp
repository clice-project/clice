#include "Compiler/Semantic.h"
#include "Index/SymbolIndex.h"

namespace clice::index {

namespace {

/// This namespace defines the binary format of the index file. Generally,
/// transform all pointer to offset to base address and cache location in the
/// location array. And data only will be deserialized when it is accessed.
namespace binary {

struct Ref {
    uint32_t value = std::numeric_limits<uint32_t>::max();

    bool isVaild() const {
        return value != std::numeric_limits<uint32_t>::max();
    }

    operator uint32_t () const {
        return value;
    }
};

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
    Ref location;
    Ref extra;
};

struct Symbol {
    int64_t id;
    String name;
    SymbolKind kind;
    Array<Relation> relations;
};

struct Occurrence {
    Ref location;
    Ref symbol;
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
    assert(relation->location.isVaild() && "Invalid location reference");
    return index->getLocations()[relation->location];
}

SymbolIndex::Symbol SymbolIndex::Relation::symbol() {
    auto index = static_cast<const binary::ProxyIndex*>(base);
    auto relation = static_cast<const binary::Relation*>(data);
    assert(relation->extra.isVaild() && "Invalid extra reference");
    return {base, &index->getSymbols()[relation->extra]};
}

int64_t SymbolIndex::SymbolID::id() {
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
    assert(occurrence->location.isVaild() && "Invalid occurrence reference");
    return index->getLocations()[occurrence->location];
}

SymbolIndex::Symbol SymbolIndex::Occurrence::symbol() {
    auto index = static_cast<const binary::ProxyIndex*>(base);
    auto occurrence = static_cast<const binary::Occurrence*>(data);
    assert(occurrence->symbol.isVaild() && "Invalid symbol reference");
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
    SymbolIndexCollector(ASTInfo& info, binary::SymbolIndex& index) : SemanticVisitor(info) {}

    struct Symbol {
        int64_t id;
        std::string name;
        SymbolKind kind;
        std::vector<binary::Relation> relations;
    };

    struct File {
        std::vector<Symbol> symbols;
        std::vector<binary::Occurrence> occurrences;
        std::vector<Location> locations;

        llvm::DenseMap<const clang::Decl*, std::size_t> symbolCache;
        llvm::DenseMap<std::pair<clang::SourceLocation, clang::SourceLocation>, std::size_t>
            locationCache;
    };

public:
    bool checkSourceRange(clang::SourceRange range, RelationKind kind) {
        assert(range.isValid() && "Invalid source range");
        auto [begin, end] = range;
        if(kind.is_one_of(RelationKind::Declaration,
                          RelationKind::Definition,
                          RelationKind::Reference)) {
            assert(begin == end && "Expect a single location");
        }
        return true;
    }

    void handleDeclOccurrence(const clang::NamedDecl* decl,
                              clang::SourceLocation location,
                              RelationKind kind) {
        println("{}", kind.name());
    }

private:
    llvm::DenseMap<clang::FileID, File> files;
};

}  // namespace

void test(ASTInfo& info) {
    binary::SymbolIndex index;
    SymbolIndexCollector collector(info, index);
    collector.run();
}

}  // namespace clice::index
