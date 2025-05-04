#include "Test/CTest.h"
#include "Index/Index2.h"

namespace clice::testing {

namespace {

using namespace clice::index::memory2;

TEST(SymbolIndex2, AddRemoveContext) {
    SymbolIndex index;

    {
        auto [context_id, context_ref] = index.addContext("test.h", 1);
        EXPECT_EQ(context_id, 0);
        EXPECT_EQ(context_ref, 0);
        EXPECT_EQ(index.header_context_count(), 1);
        EXPECT_EQ(index.unique_context_count(), 1);
    }

    {
        auto [context_id, context_ref] = index.addContext("test.h", 2);
        EXPECT_EQ(context_id, 1);
        EXPECT_EQ(context_ref, 1);
        EXPECT_EQ(index.header_context_count(), 2);
        EXPECT_EQ(index.unique_context_count(), 2);
    }

    EXPECT_EQ(index.file_count(), 1);

    {
        auto [context_id, context_ref] = index.addContext("test2.h", 1);
        EXPECT_EQ(context_id, 2);
        EXPECT_EQ(context_ref, 2);
        EXPECT_EQ(index.header_context_count(), 3);
        EXPECT_EQ(index.unique_context_count(), 3);
    }

    EXPECT_EQ(index.file_count(), 2);

    index.remove("test.h");

    EXPECT_EQ(index.header_context_count(), 1);
    EXPECT_EQ(index.unique_context_count(), 1);

    /// Test reuse context id and context ref.
    {
        auto [context_id, context_ref] = index.addContext("test3.h", 1);
        EXPECT_EQ(context_id, 0);
        EXPECT_EQ(context_ref, 0);
        EXPECT_EQ(index.header_context_count(), 2);
        EXPECT_EQ(index.unique_context_count(), 2);
    }

    {
        auto [context_id, context_ref] = index.addContext("test4.h", 1);
        EXPECT_EQ(context_id, 1);
        EXPECT_EQ(context_ref, 1);
        EXPECT_EQ(index.header_context_count(), 3);
        EXPECT_EQ(index.unique_context_count(), 3);
    }

    {
        auto [context_id, context_ref] = index.addContext("test5.h", 1);
        EXPECT_EQ(context_id, 3);
        EXPECT_EQ(context_ref, 3);
        EXPECT_EQ(index.header_context_count(), 4);
        EXPECT_EQ(index.unique_context_count(), 4);
    }

    EXPECT_EQ(index.file_count(), 4);
}

TEST(SymbolIndex2, SymbolInsert) {
    SymbolIndex index;
    index.addContext("test.h", 1);

    index.addOccurrence({1, 2}, 1);
}

TEST(SymbolIndex2, Merge) {
    SymbolIndex index;
    index.addContext("test.h", 1);

    index.addOccurrence({1, 2}, 1);

    SymbolIndex index2;
    index2.addContext("test2.h", 1);

    index2.addOccurrence({1, 2}, 1);

    index.merge(index2);

    EXPECT_EQ(index.header_context_count(), 2);
    EXPECT_EQ(index.unique_context_count(), 1);
    EXPECT_EQ(index.file_count(), 2);

    SymbolIndex index3;
    index3.addContext("test3.h", 1);

    index3.addOccurrence({1, 2}, 2);

    index.merge(index3);

    EXPECT_EQ(index.header_context_count(), 3);
    EXPECT_EQ(index.unique_context_count(), 2);
    EXPECT_EQ(index.file_count(), 3);
}

TEST(SymbolIndex2, Build) {
    llvm::StringRef context = R"(
#include <iostream>
)";

    Tester tester;
    tester.addMain("main.cpp", context);
    tester.compile();

    auto& AST = *tester.AST;
    auto indices = SymbolIndex::build(AST);
}

}  // namespace

}  // namespace clice::testing
