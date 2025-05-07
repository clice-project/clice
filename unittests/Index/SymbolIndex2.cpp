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

TEST(SymbolIndex2, MergeEmpty) {
    SymbolIndex index;
    index.addContext("test.h", 1);
    index.addOccurrence({1, 2}, 1);

    SymbolIndex index2;
    index2.addContext("test2.h", 1);

    index.merge(index2);
    EXPECT_EQ(index.header_context_count(), 2);
    EXPECT_EQ(index.unique_context_count(), 2);
    EXPECT_EQ(index.file_count(), 2);

    SymbolIndex index3;
    index3.addContext("test3.h", 1);

    index.merge(index3);
    EXPECT_EQ(index.header_context_count(), 3);
    EXPECT_EQ(index.unique_context_count(), 2);
    EXPECT_EQ(index.file_count(), 3);
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

void dump(SymbolIndex& index) {
    for(auto& context: index.contexts) {
        println("include: {}, context id: {}", context.include, context.context_id);
    }

    for(auto& [symbol_id, symbol]: index.symbols) {
        println("{} {} {}", symbol_id, symbol.kind.name(), symbol.name);
        for(auto& relation: symbol.relations) {
            println("{} {} {:#b}",
                    relation.kind.name(),
                    dump(relation.range),
                    relation.context_mask);
        }
    }

    // for(auto& [range, os]: index.occurrences) {
    //     println("{}", dump(range));
    //     for(auto occurrence: os) {
    //         println("   {} {:#b}", occurrence.target_symbol, occurrence.context_mask);
    //     }
    // }
}

TEST(SymbolIndex2, MergeReuse) {
    LocalSourceRange range = {0, 0};
    SymbolIndex index;
    index.addContext("test.h", 1);
    index.addOccurrence(range, 1);

    SymbolIndex index2;
    index2.addContext("test.h", 2);
    index2.addOccurrence(range, 1);

    SymbolIndex index3;
    index3.addContext("test.h", 3);
    index3.addOccurrence(range, 2);

    SymbolIndex index4;
    index4.addContext("test.h", 4);
    index4.addOccurrence(range, 1);

    /// Same context
    index.merge(index2);
    EXPECT_EQ(index.unique_context_count(), 1);

    /// New Context
    index.merge(index3);
    EXPECT_EQ(index.unique_context_count(), 2);

    /// Same Context
    index.merge(index4);
    EXPECT_EQ(index.unique_context_count(), 2);
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

    std::optional<SymbolIndex> base = {};

    llvm::StringRef path =
        "/home/ykiko/C++/llvm-project/build-debug-install/lib/clang/20/include/stddef.h";

    for(auto& [fid, index]: indices) {
        if(AST.getFilePath(fid) == path) {
            println("{} ---------------------------------------------", index->contexts[0].include);

            /// dump(*index);

            if(!base) {
                base = *std::move(index);
            } else {
                // if(index->contexts[0].include == 212) {
                //     println("---------------------------------------------");
                //     dump(*base);
                //     base->merge(*index);
                //     println("---------------------------------------------");
                //     dump(*base);
                //     break;
                // }

                base->merge(*index);
            }
        }
    }

    println("header count: {}, unique context: {}",
            base->header_context_count(),
            base->unique_context_count());

    /// dump(*base);

    for(auto& context: base->contexts) {
        println("include: {}, context id: {}", context.include, context.context_id);
    }
}

}  // namespace

}  // namespace clice::testing
