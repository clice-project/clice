#include "Test/Test.h"
#include "Index/Index2.h"

namespace clice::testing {

namespace {

using namespace clice::index::memory2;

TEST(Index, SymbolIndex2) {
    SymbolIndex index;
    index.addContext("test.h", 1);
    EXPECT_EQ(index.header_context_count(), 1);
    EXPECT_EQ(index.unique_context_count(), 1);

    index.getSymbol(1);
    index.getSymbol(2);
    EXPECT_EQ(index.symbol_count(), 2);

    index.addContext("test.h", 2);
    EXPECT_EQ(index.header_context_count(), 2);
    EXPECT_EQ(index.unique_context_count(), 2);

    index.remove("test.h");
    EXPECT_EQ(index.header_context_count(), 0);
    EXPECT_EQ(index.unique_context_count(), 0);
}

}  // namespace

}  // namespace clice::testing
