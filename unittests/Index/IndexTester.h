#include "../Test.h"
#include "Index/Binary.h"

namespace clice {

struct IndexTester : Tester {
    std::vector<char> binary;
    const index::binary::Index* index;

    using Tester::Tester;

    IndexTester& run(const char* standard = "-std=c++20") {
        Tester::run(standard);
        binary = index::toBinary(index::index(info));
        index = reinterpret_cast<const index::binary::Index*>(binary.data());
        return *this;
    }

    template <typename T>
    auto make_range(index::binary::Array<T> array) {
        return llvm::ArrayRef<T>(reinterpret_cast<const T*>(binary.data() + array.offset),
                                 array.size);
    }

    index::FileRef getMainFileRef() {
        auto files = make_range(index->files);
        for(auto [index, file]: views::enumerate(files)) {
            if(file.include.isInvalid()) {
                /// The include location of the main file is invalid.
                return index::FileRef{static_cast<uint32_t>(index)};
            }
        }

        /// There at least one file without include location.
        std::terminate();
    }

    const index::binary::Symbol& getSymbol(index::SymbolRef ref) {
        return make_range(index->symbols)[ref.offset];
    }

    const index::Location& getLocation(index::LocationRef ref) {
        return make_range(index->locations)[ref.offset];
    }

    llvm::StringRef getString(index::binary::String str) {
        return {binary.data() + str.offset, str.size};
    }

    std::vector<const index::binary::Symbol*> locateSymbols(index::FileRef file,
                                                            proto::Position cursor) {
        auto occurrences = make_range(index->occurrences);

        auto iter = ranges::lower_bound(occurrences, file, refl::less, [&](const auto& occurrence) {
            return getLocation(occurrence.location).file;
        });

        occurrences = {iter, occurrences.end()};

        iter = ranges::lower_bound(occurrences, cursor, refl::less, [&](const auto& occurrence) {
            return getLocation(occurrence.location).range.end;
        });

        std::vector<const index::binary::Symbol*> symbols;
        while(iter != occurrences.end()) {
            auto range = getLocation(iter->location).range;
            if(refl::less_equal(range.start, cursor) && refl::less_equal(cursor, range.end)) {
                symbols.push_back(&getSymbol(iter->symbol));
                iter += 1;
            } else {
                break;
            }
        }
        return symbols;
    }

    auto locateSymbols(llvm::StringRef cursor) {
        return locateSymbols(getMainFileRef(), locations.lookup(cursor));
    }

    IndexTester& GotoDefinition(llvm::StringRef cursor,
                                llvm::StringRef target,
                                std::source_location current = std::source_location::current()) {
        auto symbols = locateSymbols(cursor);
        EXPECT_NE(symbols.size(), 0, current);

        for(auto symbol: symbols) {
            for(auto relation: make_range(symbol->relations)) {
                if(relation.kind == RelationKind::Definition) {
                    EXPECT_EQ(getLocation(relation.location).range.start,
                              locations.lookup(target),
                              current);
                    return *this;
                }
            }
        }

        EXPECT_FAILURE("definition not found", current);
        return *this;
    }
};

}  // namespace clice
