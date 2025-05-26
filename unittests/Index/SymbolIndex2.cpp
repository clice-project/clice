#include "Test/CTest.h"
#include "Index/Index2.h"

namespace clice::testing {

namespace {

using namespace clice::index::memory2;

struct DumpConfig {
    bool enable_total = false;
    bool enable_contexts = false;
    bool enable_symbol = false;
    bool enable_occurrence = false;
};

void dump(SymbolIndex& index, DumpConfig config) {
    if(config.enable_total) {
        println("\n---------------------------Total Info---------------------------");
        println("file count: {}", index.file_count());
        println("header context count: {}", index.header_context_count());
        println("canonical context count: {}", index.canonical_context_count());
        println("symbol count: {}", index.symbols.size());
        println("occurrence count: {}", index.occurrences.size());
    }

    if(config.enable_contexts) {
        println("\n--------------------------Contexts Info-------------------------");
        for(auto& [path, contents]: index.header_contexts) {
            println("{}:", path);
            for(auto& context: contents) {
                println("   include: {}, hctx_id: {}, cctx_id: {}",
                        context.include,
                        context.hctx_id,
                        context.cctx_id);
            }
        }
    }

    if(config.enable_symbol) {
        println("\n-------------------------Symbols Info--------------------------");
        for(auto& [symbol_id, symbol]: index.symbols) {
            clice::println("symbol: {}, kind: {}", symbol.name, symbol.kind.name());
            for(auto& relation: symbol.relations) {
                if(relation.ctx.is_dependent()) {
                    auto context = index.dependent_elem_states[relation.ctx.offset()];
                    clice::println("   kind: {}, context: {:#b}",
                            relation.kind.name(),
                            context.to_ulong());
                }
            }
        }
    }

    if(config.enable_occurrence) {
        println("\n-------------------------Occurrences Info--------------------------");
        for(auto& [range, occurrences]: index.occurrences) {
            println("occurrence: {} {}", range.begin, range.end);
            for(auto& occurrence: occurrences) {
                if(occurrence.ctx.is_dependent()) {
                    auto context = index.dependent_elem_states[occurrence.ctx.offset()];
                    println("   target: {}, context: {:#b}",
                            occurrence.target_symbol,
                            context.to_ulong());
                }
            }
        }
    }
}

/// TODO: We should have a more clean way to save test data(like json), rather than out put code
/// directly.
std::string test_code(SymbolIndex& index, std::uint32_t id) {
    std::string code;
    std::string index_name = std::format("index{}", id);
    code += std::format("SymbolIndex {};", index_name);
    auto& [path, contexts] = *index.header_contexts.begin();
    code += std::format(R"({}.add_context("{}", {});)", index_name, path, contexts[0].include);
    code += "\n";

    for(auto& [symbol_id, symbol]: index.symbols) {
        code += "{";
        code += std::format(R"(
            auto& symbol = {}.getSymbol({}ull);
            symbol.name = "{}";
            symbol.kind = SymbolKind::{};
        )",
                            index_name,
                            symbol_id,
                            symbol.name,
                            symbol.kind.name());
        code += "\n";

        for(auto& relation: symbol.relations) {
            code += std::format(R"({}.addRelation(
                symbol,
                Relation{{
                    .kind = RelationKind::{},
                    .range = {{ {}, {} }},
                    .target_symbol = {}ull,
                }}    
            );)",
                                index_name,
                                relation.kind.name(),
                                relation.range.begin,
                                relation.range.end,
                                relation.target_symbol);
        }
        code += "}\n";
    }

    // for(auto& [range, occurrence_group]: index.occurrences) {
    //     code += "{";
    //     code += std::format("LocalSourceRange range{{ {}, {} }};\n", range.begin, range.end);
    //     for(auto& occurrence: occurrence_group) {
    //         code += std::format("index.addOccurrence(range, {});\n", occurrence.target_symbol);
    //     }
    //     code += "}";
    // }

    return code;
}

TEST(SymbolIndex2, AddRemoveContext) {
    SymbolIndex index;

    {
        auto context = index.add_context("test.h", 1);
        EXPECT_EQ(context.cctx_id, 0);
        EXPECT_EQ(context.hctx_id, 0);
        EXPECT_EQ(index.header_context_count(), 1);
        EXPECT_EQ(index.canonical_context_count(), 1);
    }

    {
        auto context = index.add_context("test.h", 2);
        EXPECT_EQ(context.cctx_id, 1);
        EXPECT_EQ(context.hctx_id, 1);
        EXPECT_EQ(index.header_context_count(), 2);
        EXPECT_EQ(index.canonical_context_count(), 2);
    }

    EXPECT_EQ(index.file_count(), 1);

    {
        auto context = index.add_context("test2.h", 1);
        EXPECT_EQ(context.cctx_id, 2);
        EXPECT_EQ(context.hctx_id, 2);
        EXPECT_EQ(index.header_context_count(), 3);
        EXPECT_EQ(index.canonical_context_count(), 3);
    }

    EXPECT_EQ(index.file_count(), 2);

    index.remove("test.h");

    EXPECT_EQ(index.header_context_count(), 1);
    EXPECT_EQ(index.canonical_context_count(), 1);

    /// Test reuse context id and context ref.
    {
        auto context = index.add_context("test3.h", 1);
        EXPECT_EQ(context.cctx_id, 0);
        EXPECT_EQ(context.hctx_id, 0);
        EXPECT_EQ(index.header_context_count(), 2);
        EXPECT_EQ(index.canonical_context_count(), 2);
    }

    {
        index.add_context("test4.h", 1);
        EXPECT_EQ(index.header_context_count(), 3);
        EXPECT_EQ(index.canonical_context_count(), 3);
    }

    {
        index.add_context("test5.h", 1);
        EXPECT_EQ(index.header_context_count(), 4);
        EXPECT_EQ(index.canonical_context_count(), 4);
    }

    EXPECT_EQ(index.file_count(), 4);
}

TEST(SymbolIndex2, SymbolInsert) {
    SymbolIndex index;
    index.add_context("test.h", 1);
    index.addOccurrence({1, 2}, 1);
}

TEST(SymbolIndex2, MergeEmpty) {
    SymbolIndex index;
    index.add_context("test.h", 1);
    index.addOccurrence({1, 2}, 1);

    SymbolIndex index2;
    index2.add_context("test2.h", 1);

    index.merge(index2);
    EXPECT_EQ(index.header_context_count(), 2);
    EXPECT_EQ(index.canonical_context_count(), 2);
    EXPECT_EQ(index.file_count(), 2);

    SymbolIndex index3;
    index3.add_context("test3.h", 1);

    index.merge(index3);
    EXPECT_EQ(index.header_context_count(), 3);
    EXPECT_EQ(index.canonical_context_count(), 2);
    EXPECT_EQ(index.file_count(), 3);
}

TEST(SymbolIndex2, MergeOccurrence) {
    SymbolIndex index;
    index.add_context("test.h", 1);
    index.addOccurrence({1, 2}, 1);

    SymbolIndex index2;
    index2.add_context("test2.h", 1);
    index2.addOccurrence({1, 2}, 1);

    index.merge(index2);
    EXPECT_EQ(index.header_context_count(), 2);
    EXPECT_EQ(index.canonical_context_count(), 1);
    EXPECT_EQ(index.file_count(), 2);

    SymbolIndex index3;
    index3.add_context("test3.h", 1);
    index3.addOccurrence({1, 2}, 2);

    index.merge(index3);
    EXPECT_EQ(index.header_context_count(), 3);
    EXPECT_EQ(index.canonical_context_count(), 2);
    EXPECT_EQ(index.file_count(), 3);
}

TEST(SymbolIndex2, MergeSymbol) {
    LocalSourceRange range = {0, 0};

    SymbolIndex base;
    {
        auto context = base.add_context("test.h", 1);
        auto& symbol = base.getSymbol(1);
        base.addRelation(symbol, Relation{.kind = RelationKind::Reference, .range = range});
    }

    /// Same canonical context.
    {
        SymbolIndex index;
        index.add_context("test2.h", 1);
        auto& symbol = index.getSymbol(1);
        index.addRelation(symbol, Relation{.kind = RelationKind::Reference, .range = range});

        auto context = base.merge(index);
        EXPECT_EQ(context.hctx_id, 1);
        EXPECT_EQ(context.cctx_id, 0);
        EXPECT_EQ(base.header_context_count(), 2);
        EXPECT_EQ(base.canonical_context_count(), 1);
        EXPECT_EQ(base.file_count(), 2);
    }

    /// New canonical context.
    {
        SymbolIndex index;
        index.add_context("test3.h", 1);
        auto& symbol = index.getSymbol(1);
        index.addRelation(symbol, Relation{.kind = RelationKind::Definition, .range = range});

        auto context = base.merge(index);
        EXPECT_EQ(context.hctx_id, 2);
        EXPECT_EQ(context.cctx_id, 1);
        EXPECT_EQ(base.header_context_count(), 3);
        EXPECT_EQ(base.canonical_context_count(), 2);
        EXPECT_EQ(base.file_count(), 3);
    }

    /// New canonical context.
    {
        SymbolIndex index;
        index.add_context("test4.h", 1);
        auto& symbol = index.getSymbol(1);
        index.addRelation(symbol, Relation{.kind = RelationKind::Definition, .range = range});
        index.addRelation(symbol, Relation{.kind = RelationKind::Declaration, .range = range});

        auto context = base.merge(index);
        EXPECT_EQ(context.hctx_id, 3);
        EXPECT_EQ(context.cctx_id, 2);
        EXPECT_EQ(base.header_context_count(), 4);
        EXPECT_EQ(base.canonical_context_count(), 3);
        EXPECT_EQ(base.file_count(), 4);
    }
}

TEST(SymbolIndex2, MergeReuse) {
    LocalSourceRange range = {0, 0};
    SymbolIndex index;
    index.add_context("test.h", 1);
    index.addOccurrence(range, 1);

    SymbolIndex index2;
    index2.add_context("test.h", 2);
    index2.addOccurrence(range, 1);

    SymbolIndex index3;
    index3.add_context("test.h", 3);
    index3.addOccurrence(range, 2);

    SymbolIndex index4;
    index4.add_context("test.h", 4);
    index4.addOccurrence(range, 1);

    /// Same context
    index.merge(index2);
    EXPECT_EQ(index.canonical_context_count(), 1);

    /// New Context
    index.merge(index3);
    EXPECT_EQ(index.canonical_context_count(), 2);

    /// Same Context
    index.merge(index4);
    EXPECT_EQ(index.canonical_context_count(), 2);
}

TEST(SymbolIndex2, MergeComplex) {
    SymbolIndex index1;
    index1.add_context("main.cpp", 56);
    {
        auto& symbol = index1.getSymbol(5617328926567294902ull);
        symbol.name = "__need_wchar_t";
        symbol.kind = SymbolKind::Macro;

        index1.addRelation(symbol,
                           Relation{
                               .kind = RelationKind::Definition,
                               .range = {1948, 1962},
                               .target_symbol = 8426725836700ull,
        });
        index1.addRelation(symbol,
                           Relation{
                               .kind = RelationKind::Reference,
                               .range = {4100, 4114},
                               .target_symbol = 0ull,
        });
    }
    {
        auto& symbol = index1.getSymbol(17660704465322401956ull);
        symbol.name = "__need_offsetof";
        symbol.kind = SymbolKind::Macro;

        index1.addRelation(symbol,
                           Relation{
                               .kind = RelationKind::Reference,
                               .range = {4720, 4735},
                               .target_symbol = 0ull,
        });
        index1.addRelation(symbol,
                           Relation{
                               .kind = RelationKind::Definition,
                               .range = {3433, 3448},
                               .target_symbol = 14809047240041ull,
        });
    }
    {
        auto& symbol = index1.getSymbol(12199573421319547529ull);
        symbol.name = "__need_size_t";
        symbol.kind = SymbolKind::Macro;

        index1.addRelation(symbol,
                           Relation{
                               .kind = RelationKind::Reference,
                               .range = {3867, 3880},
                               .target_symbol = 0ull,
        });
        index1.addRelation(symbol,
                           Relation{
                               .kind = RelationKind::Definition,
                               .range = {1734, 1747},
                               .target_symbol = 7503307867846ull,
        });
    }
    {
        auto& symbol = index1.getSymbol(447841485290421751ull);
        symbol.name = "__need_max_align_t";
        symbol.kind = SymbolKind::Macro;

        index1.addRelation(symbol,
                           Relation{
                               .kind = RelationKind::Definition,
                               .range = {3399, 3417},
                               .target_symbol = 14675903253831ull,
        });
        index1.addRelation(symbol,
                           Relation{
                               .kind = RelationKind::Reference,
                               .range = {4592, 4610},
                               .target_symbol = 0ull,
        });
    }
    {
        auto& symbol = index1.getSymbol(3892980363519083943ull);
        symbol.name = "__need_nullptr_t";
        symbol.kind = SymbolKind::Macro;

        index1.addRelation(symbol,
                           Relation{
                               .kind = RelationKind::Definition,
                               .range = {3138, 3154},
                               .target_symbol = 13546326854722ull,
        });
        index1.addRelation(symbol,
                           Relation{
                               .kind = RelationKind::Reference,
                               .range = {4328, 4344},
                               .target_symbol = 0ull,
        });
    }
    {
        auto& symbol = index1.getSymbol(6389328935281374692ull);
        symbol.name = "__need_NULL";
        symbol.kind = SymbolKind::Macro;

        index1.addRelation(symbol,
                           Relation{
                               .kind = RelationKind::Reference,
                               .range = {4212, 4223},
                               .target_symbol = 0ull,
        });
        index1.addRelation(symbol,
                           Relation{
                               .kind = RelationKind::Definition,
                               .range = {3005, 3016},
                               .target_symbol = 12953621367741ull,
        });
    }
    {
        auto& symbol = index1.getSymbol(13138966718646481517ull);
        symbol.name = "__cplusplus";
        symbol.kind = SymbolKind::Macro;

        index1.addRelation(symbol,
                           Relation{
                               .kind = RelationKind::Reference,
                               .range = {3367, 3378},
                               .target_symbol = 0ull,
        });
    }
    {
        auto& symbol = index1.getSymbol(4048786579988097027ull);
        symbol.name = "__need_ptrdiff_t";
        symbol.kind = SymbolKind::Macro;

        index1.addRelation(symbol,
                           Relation{
                               .kind = RelationKind::Definition,
                               .range = {1709, 1725},
                               .target_symbol = 7408818587309ull,
        });
        index1.addRelation(symbol,
                           Relation{
                               .kind = RelationKind::Reference,
                               .range = {3747, 3763},
                               .target_symbol = 0ull,
        });
    }

    SymbolIndex index2;
    index2.add_context("main.cpp", 83);
    {
        auto& symbol = index2.getSymbol(12199573421319547529ull);
        symbol.name = "__need_size_t";
        symbol.kind = SymbolKind::Macro;

        index2.addRelation(symbol,
                           Relation{
                               .kind = RelationKind::Reference,
                               .range = {3867, 3880},
                               .target_symbol = 0ull,
        });
    }
    {
        auto& symbol = index2.getSymbol(6389328935281374692ull);
        symbol.name = "__need_NULL";
        symbol.kind = SymbolKind::Macro;

        index2.addRelation(symbol,
                           Relation{
                               .kind = RelationKind::Reference,
                               .range = {4212, 4223},
                               .target_symbol = 0ull,
        });
    }

    SymbolIndex index3;
    index3.add_context("main.cpp", 87);
    {
        auto& symbol = index3.getSymbol(5617328926567294902ull);
        symbol.name = "__need_wchar_t";
        symbol.kind = SymbolKind::Macro;

        index3.addRelation(symbol,
                           Relation{
                               .kind = RelationKind::Reference,
                               .range = {4100, 4114},
                               .target_symbol = 0ull,
        });
    }
    {
        auto& symbol = index3.getSymbol(12199573421319547529ull);
        symbol.name = "__need_size_t";
        symbol.kind = SymbolKind::Macro;

        index3.addRelation(symbol,
                           Relation{
                               .kind = RelationKind::Reference,
                               .range = {3867, 3880},
                               .target_symbol = 0ull,
        });
    }
    {
        auto& symbol = index3.getSymbol(6389328935281374692ull);
        symbol.name = "__need_NULL";
        symbol.kind = SymbolKind::Macro;

        index3.addRelation(symbol,
                           Relation{
                               .kind = RelationKind::Reference,
                               .range = {4212, 4223},
                               .target_symbol = 0ull,
        });
    }

    SymbolIndex index4;
    index4.add_context("main.cpp", 118);
    {
        auto& symbol = index4.getSymbol(12199573421319547529ull);
        symbol.name = "__need_size_t";
        symbol.kind = SymbolKind::Macro;

        index4.addRelation(symbol,
                           Relation{
                               .kind = RelationKind::Reference,
                               .range = {3867, 3880},
                               .target_symbol = 0ull,
        });
    }
    {
        auto& symbol = index4.getSymbol(6389328935281374692ull);
        symbol.name = "__need_NULL";
        symbol.kind = SymbolKind::Macro;

        index4.addRelation(symbol,
                           Relation{
                               .kind = RelationKind::Reference,
                               .range = {4212, 4223},
                               .target_symbol = 0ull,
        });
    }

    SymbolIndex index5;
    index5.add_context("main.cpp", 135);
    {
        auto& symbol = index5.getSymbol(12199573421319547529ull);
        symbol.name = "__need_size_t";
        symbol.kind = SymbolKind::Macro;

        index5.addRelation(symbol,
                           Relation{
                               .kind = RelationKind::Reference,
                               .range = {3867, 3880},
                               .target_symbol = 0ull,
        });
    }

    SymbolIndex index6;
    index6.add_context("main.cpp", 147);
    {
        auto& symbol = index6.getSymbol(12199573421319547529ull);
        symbol.name = "__need_size_t";
        symbol.kind = SymbolKind::Macro;

        index6.addRelation(symbol,
                           Relation{
                               .kind = RelationKind::Reference,
                               .range = {3867, 3880},
                               .target_symbol = 0ull,
        });
    }
    {
        auto& symbol = index6.getSymbol(6389328935281374692ull);
        symbol.name = "__need_NULL";
        symbol.kind = SymbolKind::Macro;

        index6.addRelation(symbol,
                           Relation{
                               .kind = RelationKind::Reference,
                               .range = {4212, 4223},
                               .target_symbol = 0ull,
        });
    }

    SymbolIndex index7;
    index7.add_context("main.cpp", 150);
    {
        auto& symbol = index7.getSymbol(5617328926567294902ull);
        symbol.name = "__need_wchar_t";
        symbol.kind = SymbolKind::Macro;

        index7.addRelation(symbol,
                           Relation{
                               .kind = RelationKind::Definition,
                               .range = {1948, 1962},
                               .target_symbol = 8426725836700ull,
        });
        index7.addRelation(symbol,
                           Relation{
                               .kind = RelationKind::Reference,
                               .range = {4100, 4114},
                               .target_symbol = 0ull,
        });
    }
    {
        auto& symbol = index7.getSymbol(17660704465322401956ull);
        symbol.name = "__need_offsetof";
        symbol.kind = SymbolKind::Macro;

        index7.addRelation(symbol,
                           Relation{
                               .kind = RelationKind::Reference,
                               .range = {4720, 4735},
                               .target_symbol = 0ull,
        });
        index7.addRelation(symbol,
                           Relation{
                               .kind = RelationKind::Definition,
                               .range = {3433, 3448},
                               .target_symbol = 14809047240041ull,
        });
    }
    {
        auto& symbol = index7.getSymbol(12199573421319547529ull);
        symbol.name = "__need_size_t";
        symbol.kind = SymbolKind::Macro;

        index7.addRelation(symbol,
                           Relation{
                               .kind = RelationKind::Reference,
                               .range = {3867, 3880},
                               .target_symbol = 0ull,
        });
        index7.addRelation(symbol,
                           Relation{
                               .kind = RelationKind::Definition,
                               .range = {1734, 1747},
                               .target_symbol = 7503307867846ull,
        });
    }
    {
        auto& symbol = index7.getSymbol(447841485290421751ull);
        symbol.name = "__need_max_align_t";
        symbol.kind = SymbolKind::Macro;

        index7.addRelation(symbol,
                           Relation{
                               .kind = RelationKind::Definition,
                               .range = {3399, 3417},
                               .target_symbol = 14675903253831ull,
        });
        index7.addRelation(symbol,
                           Relation{
                               .kind = RelationKind::Reference,
                               .range = {4592, 4610},
                               .target_symbol = 0ull,
        });
    }
    {
        auto& symbol = index7.getSymbol(3892980363519083943ull);
        symbol.name = "__need_nullptr_t";
        symbol.kind = SymbolKind::Macro;

        index7.addRelation(symbol,
                           Relation{
                               .kind = RelationKind::Definition,
                               .range = {3138, 3154},
                               .target_symbol = 13546326854722ull,
        });
        index7.addRelation(symbol,
                           Relation{
                               .kind = RelationKind::Reference,
                               .range = {4328, 4344},
                               .target_symbol = 0ull,
        });
    }
    {
        auto& symbol = index7.getSymbol(6389328935281374692ull);
        symbol.name = "__need_NULL";
        symbol.kind = SymbolKind::Macro;

        index7.addRelation(symbol,
                           Relation{
                               .kind = RelationKind::Reference,
                               .range = {4212, 4223},
                               .target_symbol = 0ull,
        });
        index7.addRelation(symbol,
                           Relation{
                               .kind = RelationKind::Definition,
                               .range = {3005, 3016},
                               .target_symbol = 12953621367741ull,
        });
    }
    {
        auto& symbol = index7.getSymbol(13138966718646481517ull);
        symbol.name = "__cplusplus";
        symbol.kind = SymbolKind::Macro;

        index7.addRelation(symbol,
                           Relation{
                               .kind = RelationKind::Reference,
                               .range = {3367, 3378},
                               .target_symbol = 0ull,
        });
    }
    {
        auto& symbol = index7.getSymbol(4048786579988097027ull);
        symbol.name = "__need_ptrdiff_t";
        symbol.kind = SymbolKind::Macro;

        index7.addRelation(symbol,
                           Relation{
                               .kind = RelationKind::Definition,
                               .range = {1709, 1725},
                               .target_symbol = 7408818587309ull,
        });
        index7.addRelation(symbol,
                           Relation{
                               .kind = RelationKind::Reference,
                               .range = {3747, 3763},
                               .target_symbol = 0ull,
        });
    }

    SymbolIndex index8;
    index8.add_context("main.cpp", 178);
    {
        auto& symbol = index8.getSymbol(5617328926567294902ull);
        symbol.name = "__need_wchar_t";
        symbol.kind = SymbolKind::Macro;

        index8.addRelation(symbol,
                           Relation{
                               .kind = RelationKind::Reference,
                               .range = {4100, 4114},
                               .target_symbol = 0ull,
        });
    }
    {
        auto& symbol = index8.getSymbol(12199573421319547529ull);
        symbol.name = "__need_size_t";
        symbol.kind = SymbolKind::Macro;

        index8.addRelation(symbol,
                           Relation{
                               .kind = RelationKind::Reference,
                               .range = {3867, 3880},
                               .target_symbol = 0ull,
        });
    }
    {
        auto& symbol = index8.getSymbol(6389328935281374692ull);
        symbol.name = "__need_NULL";
        symbol.kind = SymbolKind::Macro;

        index8.addRelation(symbol,
                           Relation{
                               .kind = RelationKind::Reference,
                               .range = {4212, 4223},
                               .target_symbol = 0ull,
        });
    }

    SymbolIndex index9;
    index9.add_context("main.cpp", 212);
    {
        auto& symbol = index9.getSymbol(6389328935281374692ull);
        symbol.name = "__need_NULL";
        symbol.kind = SymbolKind::Macro;

        index9.addRelation(symbol,
                           Relation{
                               .kind = RelationKind::Reference,
                               .range = {4212, 4223},
                               .target_symbol = 0ull,
        });
    }

    SymbolIndex index10;
    index10.add_context("main.cpp", 226);
    {
        auto& symbol = index10.getSymbol(12199573421319547529ull);
        symbol.name = "__need_size_t";
        symbol.kind = SymbolKind::Macro;

        index10.addRelation(symbol,
                            Relation{
                                .kind = RelationKind::Reference,
                                .range = {3867, 3880},
                                .target_symbol = 0ull,
        });
    }

    {
        SymbolIndex base;
        auto context = base.merge(index1);
        EXPECT_EQ(context, SymbolIndex::HeaderContext{56, 0, 0});

        context = base.merge(index2);
        EXPECT_EQ(context, SymbolIndex::HeaderContext{83, 1, 1});

        context = base.merge(index3);
        EXPECT_EQ(context, SymbolIndex::HeaderContext{87, 2, 2});

        context = base.merge(index4);
        EXPECT_EQ(context, SymbolIndex::HeaderContext{118, 3, 1});

        context = base.merge(index5);
        EXPECT_EQ(context, SymbolIndex::HeaderContext{135, 4, 3});

        context = base.merge(index6);
        EXPECT_EQ(context, SymbolIndex::HeaderContext{147, 5, 1});

        context = base.merge(index7);
        EXPECT_EQ(context, SymbolIndex::HeaderContext{150, 6, 0});

        context = base.merge(index8);
        EXPECT_EQ(context, SymbolIndex::HeaderContext{178, 7, 2});

        context = base.merge(index9);
        EXPECT_EQ(context, SymbolIndex::HeaderContext{212, 8, 4});

        context = base.merge(index10);
        EXPECT_EQ(context, SymbolIndex::HeaderContext{226, 9, 3});
    }
}

#if 0
/// Only for local tests.
TEST(SymbolIndex2, Build) {
    llvm::StringRef context = R"(
#include <iostream>
)";

    Tester tester;
    tester.addMain("main.cpp", context);
    tester.compile();

    auto& AST = *tester.AST;
    auto indices = SymbolIndex::build(AST);

    std::optional<SymbolIndex> base = SymbolIndex();

    llvm::StringRef path =
        "/home/ykiko/C++/llvm-project/build-debug-install/lib/clang/21/include/stddef.h";

    bool print_next = false;

    std::uint32_t id = 1;
    for(auto& [fid, index]: indices) {
        if(AST.getFilePath(fid) == path) {
            /// println("{}", test_code(*index, id));
            id += 1;
            if(!base) {
                base = *std::move(index);
            } else {
                // if(print_next) {
                //     println("----------------------------------------------------------------\n");
                //     dump(*base, {.enable_symbol = true, .enable_occurrence = false});
                // }

                auto context = base->merge(*index);

                // if(print_next) {
                //     dump(*base, {.enable_symbol = true, .enable_occurrence = false});
                //     print_next = false;
                // }

                if(context.include == 118) {
                    print_next = true;
                }
            }
        }
    }

    println("----------------------------------------------------------------\n");
    dump(*base,
         {
             .enable_contexts = true,
             .enable_symbol = true,
             .enable_occurrence = true,
         });
}
#endif

}  // namespace

}  // namespace clice::testing
