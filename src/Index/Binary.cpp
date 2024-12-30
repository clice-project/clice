#include "Index.h"

namespace clice::index {

namespace {

class SymbolIndexSerder {
public:
    binary::Array<binary::Symbol> writeSymbols(llvm::ArrayRef<memory::Symbol> symbols) {
        auto begin = symbolsOffset;

        for(auto& symbol: symbols) {
            binary::Symbol binarySymbol{
                .id = symbol.id,
                .name = writeString(symbol.name),
                .kind = symbol.kind,
                .relations = writeRelations(symbol.relations),
            };
            std::memcpy(buffer + symbolsOffset, &binarySymbol, sizeof(binarySymbol));
            symbolsOffset += sizeof(binary::Symbol);
        }

        return {begin, static_cast<uint32_t>(symbols.size())};
    }

    /// FIXME: Some structs have same layout between memory and binary.

    binary::Array<binary::Occurrence>
        writeOccurrences(llvm::ArrayRef<memory::Occurrence> occurrences) {
        auto begin = occurrencesOffset;

        for(auto& occurrence: occurrences) {
            binary::Occurrence binaryOccurrence{
                .location = {occurrence.location},
                .symbol = {occurrence.symbol},
            };
            std::memcpy(buffer + occurrencesOffset, &binaryOccurrence, sizeof(binaryOccurrence));
            occurrencesOffset += sizeof(binary::Occurrence);
        }

        return {begin, static_cast<uint32_t>(occurrences.size())};
    }

    binary::Array<binary::Relation> writeRelations(llvm::ArrayRef<memory::Relation> relations) {
        auto begin = relationsOffset;
        for(auto& relation: relations) {
            binary::Relation binaryRelation{
                .kind = relation.kind,
                .data = {relation.data[0], relation.data[1]},
            };
            std::memcpy(buffer + relationsOffset, &binaryRelation, sizeof(binaryRelation));
            relationsOffset += sizeof(binary::Relation);
        }
        return {begin, static_cast<uint32_t>(relations.size())};
    }

    binary::Array<LocalSourceRange> writeRange(llvm::ArrayRef<LocalSourceRange> ranges) {
        auto begin = rangesOffset;

        for(auto& range: ranges) {
            std::memcpy(buffer + rangesOffset, &range, sizeof(LocalSourceRange));
            rangesOffset += sizeof(LocalSourceRange);
        }

        return {begin, static_cast<uint32_t>(ranges.size())};
    }

    binary::String writeString(llvm::StringRef string) {
        auto size = string.size();
        auto begin = stringsOffset;
        std::memcpy(buffer + stringsOffset, string.data(), size);
        buffer[stringsOffset + size] = '\0';
        stringsOffset += size + 1;
        return {begin, static_cast<uint32_t>(size)};
    }

    SymbolIndex build(const memory::SymbolIndex& index) {
        /// Following is the layout of the binary index format.
        /// We first calculate the size of each section and then
        /// allocate a buffer and wirte the data to the buffer.
        ///
        ///     ========================================
        ///     |              SymbolIndex             |
        ///     ========================================
        ///     |                Symbols               |
        ///     ========================================
        ///     |              Occurrences             |
        ///     ========================================
        ///     |               Relations              |
        ///     ========================================
        ///     |                Ranges                |
        ///     ========================================
        ///     |                Strings               |
        ///     ========================================

        symbolsOffset = sizeof(binary::SymbolIndex);

        occurrencesOffset = symbolsOffset + index.symbols.size() * sizeof(binary::Symbol);

        relationsOffset = occurrencesOffset + index.occurrences.size() * sizeof(binary::Occurrence);

        rangesOffset = relationsOffset;
        for(auto& symbol: index.symbols) {
            rangesOffset += symbol.relations.size() * sizeof(binary::Relation);
        }

        stringsOffset = rangesOffset + index.ranges.size() * sizeof(LocalSourceRange);

        totalSize = stringsOffset;
        for(auto& symbol: index.symbols) {
            totalSize += symbol.name.size() + 1;
        }

        buffer = static_cast<char*>(std::malloc(totalSize));

        /// There are several padding in structs, And because we use need
        /// to compare the binary index for megere, fill the buffer with 0.
        std::memset(buffer, 0, totalSize);

        auto binary = new (buffer) binary::SymbolIndex{};
        binary->symbols = writeSymbols(index.symbols);
        binary->occurrences = writeOccurrences(index.occurrences);
        binary->ranges = writeRange(index.ranges);
        return SymbolIndex{buffer, totalSize};
    }

private:
    uint32_t symbolsOffset = 0;
    uint32_t occurrencesOffset = 0;
    uint32_t relationsOffset = 0;
    uint32_t rangesOffset = 0;
    uint32_t stringsOffset = 0;
    std::size_t totalSize = 0;

    char* buffer = nullptr;
};

}  // namespace

SymbolIndex serialize(const memory::SymbolIndex& index) {
    SymbolIndexSerder serder;
    return serder.build(index);
}

binary::FeatureIndex* serialize(const memory::FeatureIndex& index) {
    return nullptr;
}

}  // namespace clice::index
