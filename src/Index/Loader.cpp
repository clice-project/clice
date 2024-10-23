#include <Index/Loader.h>

namespace clice::index {

const out::Symbol& Loader::locate(in::SymbolID symbolID) {}

in::SymbolID Loader::locate(in::Location location) {
    auto [begin, end] = make_range(index.occurrences);
    auto result = std::partition_point(begin, end, [&](const out::Occurrence& occurrence) {
        auto& loc = access(occurrence.location);
        in::Location decodedLoc = {loc.begin, loc.end, make_string(loc.file)};
        llvm::outs() << "Decoded location: " << decodedLoc.begin.line << " " << decodedLoc.begin.column
                     << decodedLoc.file << "\n";
        return decodedLoc < location;
    });

    if(result == end) {
        std::terminate();
    }

    auto& outID = access(result->symbol);
    return in::SymbolID{outID.value, make_string(outID.USR)};
}

}  // namespace clice::index
