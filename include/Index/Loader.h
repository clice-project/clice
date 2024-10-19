#pragma once

#include <Index/Index.h>

namespace clice {

class Loader {
public:
    Loader(CSIF csif, char* data) : csif(csif), data(data) {}

    const Symbol& locate(Location loc) const {
        auto iter = std::partition_point(csif.occurrences.begin(), csif.occurrences.end(), [&](const auto& occurrence) {
            return occurrence.location < loc;
        });

        if(iter == csif.occurrences.end()) {
            std::terminate();
        }

        auto id = iter->symbol;
        auto symbol = std::partition_point(csif.symbols.begin(), csif.symbols.end(), [&](const auto& symbol) {
            return symbol.ID < id;
        });

        if(symbol == csif.symbols.end()) {
            std::terminate();
        }

        return *symbol;
    }

private:
    CSIF csif;
    char* data;
};

}  // namespace clice
