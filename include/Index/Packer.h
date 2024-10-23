#pragma once

#include "Index.h"

namespace clice::index {

class Packer {
public:
    Packer() = default;

    std::vector<char> pack(in::Index index);

private:
    struct Layout {
        std::size_t binarySize;
        std::size_t symbolBegin, symbolCount;
        std::size_t occurrenceBegin, occurrenceCount;
        std::size_t relationBegin, relationCount;
        std::size_t symbolIDBegin, symbolIDCount;
        std::size_t locationBegin, locationCount;
        std::size_t stringBegin, stringCount;
        std::size_t charBegin;

        static Layout from(const in::Index& index);
    };

    template <typename In, typename Out>
    void pack(const In& in, Out& out) {
        refl::foreach(in, out, [&](const auto& inField, auto& outField) {
            if constexpr(requires { outField = inField; }) {
                outField = inField;
            } else {
                pack(inField, outField);
            }
        });
    }

    void pack(in::ArrayRef<in::Symbol> in, out::ArrayRef<out::Symbol>& out);
    void pack(in::ArrayRef<in::Occurrence> in, out::ArrayRef<out::Occurrence>& out);
    void pack(in::ArrayRef<in::Relation> in, out::ArrayRef<out::Relation>& out);
    void pack(in::ArrayRef<in::StringRef> in, out::ArrayRef<out::StringRef>& out);
    void pack(in::SymbolID in, out::Ref<out::SymbolID>& out);
    void pack(in::Location in, out::Ref<out::Location>& out);
    void pack(in::StringRef in, out::StringRef& out);

private:
    Layout layout;
    std::vector<out::Symbol> symbols;
    std::vector<out::Occurrence> occurrences;
    std::vector<out::Relation> relations;
    std::vector<out::StringRef> strings;
    std::vector<out::SymbolID> symbolIDs;
    std::vector<out::Location> locations;
    std::vector<char> chars;
};

}  // namespace clice::index
