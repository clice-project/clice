#include "Test/Tester.h"
#include "Index/SymbolIndex.h"

namespace clice::testing {

struct IndexTester : Tester {
    std::vector<char> binary;
    llvm::DenseMap<clang::FileID, std::vector<char>> indices;
    using Tester::Tester;

    IndexTester& run(const char* standard = "-std=c++20") {
        compile(standard);
        /// indices = index::index(*info);
        return *this;
    }

    IndexTester& GotoDefinition(llvm::StringRef cursor,
                                llvm::StringRef target,
                                LocationChain chain = LocationChain()) {
        /// SourceConverter converter;
        ///  auto offset = converter.toOffset(sources[0], pos(cursor));

        /// llvm::SmallVector<index::SymbolIndex::Symbol> symbols;
        /// indices.begin()->second.locateSymbols(offset, symbols);
        /// EXPECT_EQ(symbols.size(), 1, current);

        /// bool found = false;
        /// for(auto&& relation: symbols[0].relations()) {
        ///     if(relation.kind() & RelationKind::Definition) {
        ///         EXPECT_EQ(relation.range()->begin, offsets[target], current);
        ///         found = true;
        ///     }
        /// }
        /// EXPECT_TRUE(found);

        return *this;
    }
};

}  // namespace clice::testing
