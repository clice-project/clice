#pragma once

#include <vector>
#include <iostream>
#include <source_location>

namespace clice::testing {

struct LocationChain {
    /// All positions in the call chain, with the later
    /// ones representing deeper inner levels.
    std::vector<std::source_location> locations;

    LocationChain(std::source_location current = std::source_location::current()) :
        locations{current} {}

    LocationChain(LocationChain& outer,
                  std::source_location current = std::source_location::current()) :
        locations{outer.locations} {
        locations.emplace_back(current);
    }

    LocationChain(const LocationChain&) = delete;

    /// Dump all locations.
    void backtrace() {
        for(auto location: locations) {
            std::cout << location.file_name() << ":" << location.line() << ":" << location.column()
                      << "\n";
        }
    }
};

}  // namespace clice::testing
