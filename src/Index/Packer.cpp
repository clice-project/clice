#include <Index/Packer.h>

namespace clice::index {

Packer::Layout Packer::Layout::from(const in::Index& index) {
    Layout layout = {};
    // `out::Index` is located at the beginning of the binary.
    layout.binarySize += sizeof(out::Index);

    layout.symbolBegin = layout.binarySize;
    layout.symbolCount = index.symbols.size();
    layout.binarySize += layout.symbolCount * sizeof(out::Symbol);

    layout.occurrenceBegin = layout.binarySize;
    layout.occurrenceCount = index.occurrences.size();
    layout.binarySize += layout.occurrenceCount * sizeof(out::Occurrence);

    layout.relationBegin = layout.binarySize;
    layout.relationCount = 0;
    for(auto& symbol: index.symbols) {
        layout.relationCount += symbol.relations.size();
    }
    layout.binarySize += layout.relationCount * sizeof(out::Relation);

    layout.stringBegin = layout.binarySize;
    layout.stringCount = index.commands.size();
    layout.binarySize += layout.stringCount * sizeof(out::StringRef);

    layout.symbolIDBegin = layout.binarySize;
    layout.symbolIDCount = index.symbols.size() + index.occurrences.size();
    layout.binarySize += layout.symbolIDCount * sizeof(out::SymbolID);

    layout.locationBegin = layout.binarySize;
    layout.locationCount = index.occurrences.size() + layout.relationCount;
    layout.binarySize += layout.locationCount * sizeof(out::Location);

    // Because char buffer is located at the end of the binary, we
    // don't need to calculate the size of it.
    layout.charBegin = layout.binarySize;
    return layout;
}

void Packer::pack(in::ArrayRef<in::Symbol> in, out::ArrayRef<out::Symbol>& out) {
    out.offset = layout.symbolBegin + symbols.size() * sizeof(out::Symbol);
    out.length = layout.symbolCount;
    for(auto& symbol: in) {
        pack(symbol, symbols.emplace_back());
    }
}

void Packer::pack(in::ArrayRef<in::Occurrence> in, out::ArrayRef<out::Occurrence>& out) {
    out.offset = layout.occurrenceBegin + occurrences.size() * sizeof(out::Occurrence);
    out.length = layout.occurrenceCount;
    for(auto& occurrence: in) {
        pack(occurrence, occurrences.emplace_back());
    }
}

void Packer::pack(in::ArrayRef<in::Relation> in, out::ArrayRef<out::Relation>& out) {
    out.offset = layout.relationBegin + relations.size() * sizeof(out::Relation);
    out.length = layout.relationCount;
    for(auto& relation: in) {
        pack(relation, relations.emplace_back());
    }
}

void Packer::pack(in::ArrayRef<in::StringRef> in, out::ArrayRef<out::StringRef>& out) {
    out.offset = layout.stringBegin + strings.size() * sizeof(out::StringRef);
    out.length = layout.stringCount;
    for(auto& string: in) {
        pack(string, strings.emplace_back());
    }
}

void Packer::pack(in::SymbolID in, out::Ref<out::SymbolID>& out) {
    out.offset = layout.symbolIDBegin + symbolIDs.size() * sizeof(out::SymbolID);
    pack(in, symbolIDs.emplace_back());
}

void Packer::pack(in::Location in, out::Ref<out::Location>& out) {
    out.offset = layout.locationBegin + locations.size() * sizeof(out::Location);
    pack(in, locations.emplace_back());
}

void Packer::pack(in::StringRef in, out::StringRef& out) {
    out.offset = layout.charBegin + chars.size();
    out.length = in.size();
    chars.insert(chars.end(), in.begin(), in.end());
    chars.push_back('\0');
}

std::vector<char> Packer::pack(in::Index inIndex) {
    layout = Layout::from(inIndex);
    symbols.reserve(layout.symbolCount);
    occurrences.reserve(layout.occurrenceCount);
    strings.reserve(layout.stringCount);
    relations.reserve(layout.relationCount);
    symbolIDs.reserve(layout.symbolIDCount);
    locations.reserve(layout.locationCount);

    out::Index outIndex = {};
    pack(inIndex, outIndex);

    if(symbols.size() != layout.symbolCount || occurrences.size() != layout.occurrenceCount ||
       relations.size() != layout.relationCount || symbolIDs.size() != layout.symbolIDCount ||
       locations.size() != layout.locationCount || strings.size() != layout.stringCount) {
        throw std::runtime_error("Packing failed");
    }

    std::vector<char> binary = {};
    binary.reserve(layout.binarySize);
    binary.insert(binary.end(), reinterpret_cast<char*>(&outIndex), reinterpret_cast<char*>(&outIndex + 1));
    binary.insert(binary.end(),
                  reinterpret_cast<char*>(symbols.data()),
                  reinterpret_cast<char*>(symbols.data() + symbols.size()));
    binary.insert(binary.end(),
                  reinterpret_cast<char*>(occurrences.data()),
                  reinterpret_cast<char*>(occurrences.data() + occurrences.size()));
    binary.insert(binary.end(),
                  reinterpret_cast<char*>(relations.data()),
                  reinterpret_cast<char*>(relations.data() + relations.size()));
    binary.insert(binary.end(),
                  reinterpret_cast<char*>(symbolIDs.data()),
                  reinterpret_cast<char*>(symbolIDs.data() + symbolIDs.size()));
    binary.insert(binary.end(),
                  reinterpret_cast<char*>(locations.data()),
                  reinterpret_cast<char*>(locations.data() + locations.size()));
    binary.insert(binary.end(),
                  reinterpret_cast<char*>(strings.data()),
                  reinterpret_cast<char*>(strings.data() + strings.size()));
    binary.insert(binary.end(), chars.begin(), chars.end());
    return binary;
}

}  // namespace clice::index
