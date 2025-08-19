#include "Test/Tester.h"
#include "Index/HeaderIndex.h"

namespace clice::testing {

namespace {

using namespace clice::index::memory;

struct DumpConfig {
    bool enable_total = false;
    bool enable_contexts = false;
    bool enable_symbol = false;
    bool enable_occurrence = false;
};

auto dump = [](HeaderIndex& index, DumpConfig config) {
    if(config.enable_total) {
        std::println("\n---------------------------Total Info---------------------------");
        std::println("file count: {}", index.file_count());
        std::println("header context count: {}", index.header_context_count());
        std::println("canonical context count: {}", index.canonical_context_count());
        std::println("symbol count: {}", index.symbols.size());
        std::println("occurrence count: {}", index.occurrences.size());
    }

    if(config.enable_contexts) {
        std::println("\n--------------------------Contexts Info-------------------------");
        for(auto& [path, contents]: index.header_contexts) {
            std::println("{}:", path);
            for(auto& context: contents) {
                std::println("   include: {}, hctx_id: {}, cctx_id: {}",
                             context.include,
                             context.hctx_id,
                             context.cctx_id);
            }
        }
    }

    if(config.enable_symbol) {
        std::println("\n-------------------------Symbols Info--------------------------");
        for(auto& [symbol_id, symbol]: index.symbols) {
            std::println("symbol: {}, kind: {}", symbol.name, symbol.kind.name());
            for(auto& relation: symbol.relations) {
                if(relation.ctx.is_dependent()) {
                    auto context = index.dependent_elem_states[relation.ctx.offset()];
                    std::println("   kind: {}, context: {:#b}",
                                 relation.kind.name(),
                                 context.to_ulong());
                }
            }
        }
    }

    if(config.enable_occurrence) {
        std::println("\n-------------------------Occurrences Info--------------------------");
        for(auto& [range, occurrences]: index.occurrences) {
            std::println("occurrence: {} {}", range.begin, range.end);
            for(auto& occurrence: occurrences) {
                if(occurrence.ctx.is_dependent()) {
                    auto context = index.dependent_elem_states[occurrence.ctx.offset()];
                    std::println("   target: {}, context: {:#b}",
                                 occurrence.target_symbol,
                                 context.to_ulong());
                }
            }
        }
    }
};

/// TODO: We should have a more clean way to save test data(like json), rather than out put code
/// directly.
auto test_code = [](HeaderIndex& index, std::uint32_t id) {
    std::string code;
    std::string index_name = std::format("index{}", id);
    code += std::format("RawIndex {};", index_name);
    auto& [path, contexts] = *index.header_contexts.begin();
    code += std::format(R"({}.add_context("{}", {});)", index_name, path, contexts[0].include);
    code += "\n";

    for(auto& [symbol_id, symbol]: index.symbols) {
        code += "{";
        code += std::format(R"(
                auto& symbol = {}.get_symbol({}ull);
                symbol.name = "{}";
                symbol.kind = SymbolKind::{};
            )",
                            index_name,
                            symbol_id,
                            symbol.name,
                            symbol.kind.name());
        code += "\n";

        for(auto& relation: symbol.relations) {
            code += std::format(R"({}.add_relation(
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
    //         code += std::format("index.add_occurrence(range, {});\n",
    //         occurrence.target_symbol);
    //     }
    //     code += "}";
    // }

    return code;
};

suite<"HeaderIndex"> header_index = [] {
    test("AddRemoveContext") = [&] {
        HeaderIndex index;

        {
            auto context = index.add_context("test.h", 1);
            expect(that % context.cctx_id == 0);
            expect(that % context.hctx_id == 0);
            expect(that % index.header_context_count() == 1);
            expect(that % index.canonical_context_count() == 1);
        }

        {
            auto context = index.add_context("test.h", 2);
            expect(that % context.cctx_id == 1);
            expect(that % context.hctx_id == 1);
            expect(that % index.header_context_count() == 2);
            expect(that % index.canonical_context_count() == 2);
        }

        expect(that % index.file_count() == 1);

        {
            auto context = index.add_context("test2.h", 1);
            expect(that % context.cctx_id == 2);
            expect(that % context.hctx_id == 2);
            expect(that % index.header_context_count() == 3);
            expect(that % index.canonical_context_count() == 3);
        }

        expect(that % index.file_count() == 2);

        index.remove("test.h");

        expect(that % index.header_context_count() == 1);
        expect(that % index.canonical_context_count() == 1);

        /// Test reuse context id and context ref.
        {
            auto context = index.add_context("test3.h", 1);
            expect(that % context.cctx_id == 0);
            expect(that % context.hctx_id == 0);
            expect(that % index.header_context_count() == 2);
            expect(that % index.canonical_context_count() == 2);
        }

        {
            index.add_context("test4.h", 1);
            expect(that % index.header_context_count() == 3);
            expect(that % index.canonical_context_count() == 3);
        }

        {
            index.add_context("test5.h", 1);
            expect(that % index.header_context_count() == 4);
            expect(that % index.canonical_context_count() == 4);
        }

        expect(that % index.file_count() == 4);
    };

    test("SymbolInsert") = [&] {
        HeaderIndex index;
        index.add_context("test.h", 1);
        index.add_occurrence({1, 2}, 1);
    };

    test("MergeEmpty") = [&] {
        HeaderIndex base;

        RawIndex index;
        index.add_occurrence({1, 2}, 1);
        base.merge("test.h", 1, index);
        expect(that % base.header_context_count() == 1);
        expect(that % base.canonical_context_count() == 1);
        expect(that % base.file_count() == 1);

        RawIndex index2;
        base.merge("test2.h", 1, index2);
        expect(that % base.header_context_count() == 2);
        expect(that % base.canonical_context_count() == 2);
        expect(that % base.file_count() == 2);

        RawIndex index3;
        base.merge("test3.h", 1, index3);
        expect(that % base.header_context_count() == 3);
        expect(that % base.canonical_context_count() == 2);
        expect(that % base.file_count() == 3);
    };

    test("MergeOccurrence") = [&] {
        HeaderIndex base;

        RawIndex index;
        index.add_occurrence({1, 2}, 1);
        base.merge("test.h", 1, index);

        RawIndex index2;
        index2.add_occurrence({1, 2}, 1);
        base.merge("test2.h", 1, index2);
        expect(that % base.header_context_count() == 2);
        expect(that % base.canonical_context_count() == 1);
        expect(that % base.file_count() == 2);

        RawIndex index3;
        index3.add_occurrence({1, 2}, 2);
        base.merge("test3.h", 1, index3);
        expect(that % base.header_context_count() == 3);
        expect(that % base.canonical_context_count() == 2);
        expect(that % base.file_count() == 3);
    };

    test("MergeSymbol") = [&] {
        LocalSourceRange range = {0, 0};

        HeaderIndex base;
        {
            RawIndex index;
            auto& symbol = index.get_symbol(1);
            index.add_relation(symbol, Relation{.kind = RelationKind::Reference, .range = range});
            base.merge("test.h", 1, index);
        }

        /// Same canonical context.
        {
            RawIndex index;
            auto& symbol = index.get_symbol(1);
            index.add_relation(symbol, Relation{.kind = RelationKind::Reference, .range = range});
            auto context = base.merge("test2.h", 1, index);
            expect(that % context.hctx_id == 1);
            expect(that % context.cctx_id == 0);
            expect(that % base.header_context_count() == 2);
            expect(that % base.canonical_context_count() == 1);
            expect(that % base.file_count() == 2);
        }

        /// New canonical context.
        {
            RawIndex index;
            auto& symbol = index.get_symbol(1);
            index.add_relation(symbol, Relation{.kind = RelationKind::Definition, .range = range});

            auto context = base.merge("test3.h", 1, index);
            expect(that % context.hctx_id == 2);
            expect(that % context.cctx_id == 1);
            expect(that % base.header_context_count() == 3);
            expect(that % base.canonical_context_count() == 2);
            expect(that % base.file_count() == 3);
        }

        /// New canonical context.
        {
            RawIndex index;
            auto& symbol = index.get_symbol(1);
            index.add_relation(symbol, Relation{.kind = RelationKind::Definition, .range = range});
            index.add_relation(symbol, Relation{.kind = RelationKind::Declaration, .range = range});

            auto context = base.merge("test4.h", 1, index);
            expect(that % context.hctx_id == 3);
            expect(that % context.cctx_id == 2);
            expect(that % base.header_context_count() == 4);
            expect(that % base.canonical_context_count() == 3);
            expect(that % base.file_count() == 4);
        }
    };

    test("MergeReuse") = [&] {
        LocalSourceRange range = {0, 0};
        HeaderIndex base;

        RawIndex index;
        index.add_occurrence(range, 1);
        base.merge("test.h", 1, index);

        /// Same context
        RawIndex index2;
        index2.add_occurrence(range, 1);
        base.merge("test.h", 2, index2);
        expect(that % base.canonical_context_count() == 1);

        /// New Context
        RawIndex index3;
        index3.add_occurrence(range, 2);
        base.merge("test.h", 3, index3);
        expect(that % base.canonical_context_count() == 2);

        /// Same Context
        RawIndex index4;
        index4.add_occurrence(range, 1);
        base.merge("test.h", 4, index4);
        expect(that % base.canonical_context_count() == 2);
    };

    test("MergeComplex") = [] {
        RawIndex index1;
        {
            auto& symbol = index1.get_symbol(5617328926567294902ull);
            symbol.name = "__need_wchar_t";
            symbol.kind = SymbolKind::Macro;

            index1.add_relation(symbol,
                                Relation{
                                    .kind = RelationKind::Definition,
                                    .range = {1948, 1962},
                                    .target_symbol = 8426725836700ull,
            });
            index1.add_relation(symbol,
                                Relation{
                                    .kind = RelationKind::Reference,
                                    .range = {4100, 4114},
                                    .target_symbol = 0ull,
            });
        }
        {
            auto& symbol = index1.get_symbol(17660704465322401956ull);
            symbol.name = "__need_offsetof";
            symbol.kind = SymbolKind::Macro;

            index1.add_relation(symbol,
                                Relation{
                                    .kind = RelationKind::Reference,
                                    .range = {4720, 4735},
                                    .target_symbol = 0ull,
            });
            index1.add_relation(symbol,
                                Relation{
                                    .kind = RelationKind::Definition,
                                    .range = {3433, 3448},
                                    .target_symbol = 14809047240041ull,
            });
        }
        {
            auto& symbol = index1.get_symbol(12199573421319547529ull);
            symbol.name = "__need_size_t";
            symbol.kind = SymbolKind::Macro;

            index1.add_relation(symbol,
                                Relation{
                                    .kind = RelationKind::Reference,
                                    .range = {3867, 3880},
                                    .target_symbol = 0ull,
            });
            index1.add_relation(symbol,
                                Relation{
                                    .kind = RelationKind::Definition,
                                    .range = {1734, 1747},
                                    .target_symbol = 7503307867846ull,
            });
        }
        {
            auto& symbol = index1.get_symbol(447841485290421751ull);
            symbol.name = "__need_max_align_t";
            symbol.kind = SymbolKind::Macro;

            index1.add_relation(symbol,
                                Relation{
                                    .kind = RelationKind::Definition,
                                    .range = {3399, 3417},
                                    .target_symbol = 14675903253831ull,
            });
            index1.add_relation(symbol,
                                Relation{
                                    .kind = RelationKind::Reference,
                                    .range = {4592, 4610},
                                    .target_symbol = 0ull,
            });
        }
        {
            auto& symbol = index1.get_symbol(3892980363519083943ull);
            symbol.name = "__need_nullptr_t";
            symbol.kind = SymbolKind::Macro;

            index1.add_relation(symbol,
                                Relation{
                                    .kind = RelationKind::Definition,
                                    .range = {3138, 3154},
                                    .target_symbol = 13546326854722ull,
            });
            index1.add_relation(symbol,
                                Relation{
                                    .kind = RelationKind::Reference,
                                    .range = {4328, 4344},
                                    .target_symbol = 0ull,
            });
        }
        {
            auto& symbol = index1.get_symbol(6389328935281374692ull);
            symbol.name = "__need_NULL";
            symbol.kind = SymbolKind::Macro;

            index1.add_relation(symbol,
                                Relation{
                                    .kind = RelationKind::Reference,
                                    .range = {4212, 4223},
                                    .target_symbol = 0ull,
            });
            index1.add_relation(symbol,
                                Relation{
                                    .kind = RelationKind::Definition,
                                    .range = {3005, 3016},
                                    .target_symbol = 12953621367741ull,
            });
        }
        {
            auto& symbol = index1.get_symbol(13138966718646481517ull);
            symbol.name = "__cplusplus";
            symbol.kind = SymbolKind::Macro;

            index1.add_relation(symbol,
                                Relation{
                                    .kind = RelationKind::Reference,
                                    .range = {3367, 3378},
                                    .target_symbol = 0ull,
            });
        }
        {
            auto& symbol = index1.get_symbol(4048786579988097027ull);
            symbol.name = "__need_ptrdiff_t";
            symbol.kind = SymbolKind::Macro;

            index1.add_relation(symbol,
                                Relation{
                                    .kind = RelationKind::Definition,
                                    .range = {1709, 1725},
                                    .target_symbol = 7408818587309ull,
            });
            index1.add_relation(symbol,
                                Relation{
                                    .kind = RelationKind::Reference,
                                    .range = {3747, 3763},
                                    .target_symbol = 0ull,
            });
        }

        RawIndex index2;
        {
            auto& symbol = index2.get_symbol(12199573421319547529ull);
            symbol.name = "__need_size_t";
            symbol.kind = SymbolKind::Macro;

            index2.add_relation(symbol,
                                Relation{
                                    .kind = RelationKind::Reference,
                                    .range = {3867, 3880},
                                    .target_symbol = 0ull,
            });
        }
        {
            auto& symbol = index2.get_symbol(6389328935281374692ull);
            symbol.name = "__need_NULL";
            symbol.kind = SymbolKind::Macro;

            index2.add_relation(symbol,
                                Relation{
                                    .kind = RelationKind::Reference,
                                    .range = {4212, 4223},
                                    .target_symbol = 0ull,
            });
        }

        RawIndex index3;

        {
            auto& symbol = index3.get_symbol(5617328926567294902ull);
            symbol.name = "__need_wchar_t";
            symbol.kind = SymbolKind::Macro;

            index3.add_relation(symbol,
                                Relation{
                                    .kind = RelationKind::Reference,
                                    .range = {4100, 4114},
                                    .target_symbol = 0ull,
            });
        }
        {
            auto& symbol = index3.get_symbol(12199573421319547529ull);
            symbol.name = "__need_size_t";
            symbol.kind = SymbolKind::Macro;

            index3.add_relation(symbol,
                                Relation{
                                    .kind = RelationKind::Reference,
                                    .range = {3867, 3880},
                                    .target_symbol = 0ull,
            });
        }
        {
            auto& symbol = index3.get_symbol(6389328935281374692ull);
            symbol.name = "__need_NULL";
            symbol.kind = SymbolKind::Macro;

            index3.add_relation(symbol,
                                Relation{
                                    .kind = RelationKind::Reference,
                                    .range = {4212, 4223},
                                    .target_symbol = 0ull,
            });
        }

        RawIndex index4;
        {
            auto& symbol = index4.get_symbol(12199573421319547529ull);
            symbol.name = "__need_size_t";
            symbol.kind = SymbolKind::Macro;

            index4.add_relation(symbol,
                                Relation{
                                    .kind = RelationKind::Reference,
                                    .range = {3867, 3880},
                                    .target_symbol = 0ull,
            });
        }
        {
            auto& symbol = index4.get_symbol(6389328935281374692ull);
            symbol.name = "__need_NULL";
            symbol.kind = SymbolKind::Macro;

            index4.add_relation(symbol,
                                Relation{
                                    .kind = RelationKind::Reference,
                                    .range = {4212, 4223},
                                    .target_symbol = 0ull,
            });
        }

        RawIndex index5;
        {
            auto& symbol = index5.get_symbol(12199573421319547529ull);
            symbol.name = "__need_size_t";
            symbol.kind = SymbolKind::Macro;

            index5.add_relation(symbol,
                                Relation{
                                    .kind = RelationKind::Reference,
                                    .range = {3867, 3880},
                                    .target_symbol = 0ull,
            });
        }

        RawIndex index6;
        {
            auto& symbol = index6.get_symbol(12199573421319547529ull);
            symbol.name = "__need_size_t";
            symbol.kind = SymbolKind::Macro;

            index6.add_relation(symbol,
                                Relation{
                                    .kind = RelationKind::Reference,
                                    .range = {3867, 3880},
                                    .target_symbol = 0ull,
            });
        }
        {
            auto& symbol = index6.get_symbol(6389328935281374692ull);
            symbol.name = "__need_NULL";
            symbol.kind = SymbolKind::Macro;

            index6.add_relation(symbol,
                                Relation{
                                    .kind = RelationKind::Reference,
                                    .range = {4212, 4223},
                                    .target_symbol = 0ull,
            });
        }

        RawIndex index7;
        {
            auto& symbol = index7.get_symbol(5617328926567294902ull);
            symbol.name = "__need_wchar_t";
            symbol.kind = SymbolKind::Macro;

            index7.add_relation(symbol,
                                Relation{
                                    .kind = RelationKind::Definition,
                                    .range = {1948, 1962},
                                    .target_symbol = 8426725836700ull,
            });
            index7.add_relation(symbol,
                                Relation{
                                    .kind = RelationKind::Reference,
                                    .range = {4100, 4114},
                                    .target_symbol = 0ull,
            });
        }
        {
            auto& symbol = index7.get_symbol(17660704465322401956ull);
            symbol.name = "__need_offsetof";
            symbol.kind = SymbolKind::Macro;

            index7.add_relation(symbol,
                                Relation{
                                    .kind = RelationKind::Reference,
                                    .range = {4720, 4735},
                                    .target_symbol = 0ull,
            });
            index7.add_relation(symbol,
                                Relation{
                                    .kind = RelationKind::Definition,
                                    .range = {3433, 3448},
                                    .target_symbol = 14809047240041ull,
            });
        }
        {
            auto& symbol = index7.get_symbol(12199573421319547529ull);
            symbol.name = "__need_size_t";
            symbol.kind = SymbolKind::Macro;

            index7.add_relation(symbol,
                                Relation{
                                    .kind = RelationKind::Reference,
                                    .range = {3867, 3880},
                                    .target_symbol = 0ull,
            });
            index7.add_relation(symbol,
                                Relation{
                                    .kind = RelationKind::Definition,
                                    .range = {1734, 1747},
                                    .target_symbol = 7503307867846ull,
            });
        }
        {
            auto& symbol = index7.get_symbol(447841485290421751ull);
            symbol.name = "__need_max_align_t";
            symbol.kind = SymbolKind::Macro;

            index7.add_relation(symbol,
                                Relation{
                                    .kind = RelationKind::Definition,
                                    .range = {3399, 3417},
                                    .target_symbol = 14675903253831ull,
            });
            index7.add_relation(symbol,
                                Relation{
                                    .kind = RelationKind::Reference,
                                    .range = {4592, 4610},
                                    .target_symbol = 0ull,
            });
        }
        {
            auto& symbol = index7.get_symbol(3892980363519083943ull);
            symbol.name = "__need_nullptr_t";
            symbol.kind = SymbolKind::Macro;

            index7.add_relation(symbol,
                                Relation{
                                    .kind = RelationKind::Definition,
                                    .range = {3138, 3154},
                                    .target_symbol = 13546326854722ull,
            });
            index7.add_relation(symbol,
                                Relation{
                                    .kind = RelationKind::Reference,
                                    .range = {4328, 4344},
                                    .target_symbol = 0ull,
            });
        }
        {
            auto& symbol = index7.get_symbol(6389328935281374692ull);
            symbol.name = "__need_NULL";
            symbol.kind = SymbolKind::Macro;

            index7.add_relation(symbol,
                                Relation{
                                    .kind = RelationKind::Reference,
                                    .range = {4212, 4223},
                                    .target_symbol = 0ull,
            });
            index7.add_relation(symbol,
                                Relation{
                                    .kind = RelationKind::Definition,
                                    .range = {3005, 3016},
                                    .target_symbol = 12953621367741ull,
            });
        }
        {
            auto& symbol = index7.get_symbol(13138966718646481517ull);
            symbol.name = "__cplusplus";
            symbol.kind = SymbolKind::Macro;

            index7.add_relation(symbol,
                                Relation{
                                    .kind = RelationKind::Reference,
                                    .range = {3367, 3378},
                                    .target_symbol = 0ull,
            });
        }
        {
            auto& symbol = index7.get_symbol(4048786579988097027ull);
            symbol.name = "__need_ptrdiff_t";
            symbol.kind = SymbolKind::Macro;

            index7.add_relation(symbol,
                                Relation{
                                    .kind = RelationKind::Definition,
                                    .range = {1709, 1725},
                                    .target_symbol = 7408818587309ull,
            });
            index7.add_relation(symbol,
                                Relation{
                                    .kind = RelationKind::Reference,
                                    .range = {3747, 3763},
                                    .target_symbol = 0ull,
            });
        }

        RawIndex index8;
        {
            auto& symbol = index8.get_symbol(5617328926567294902ull);
            symbol.name = "__need_wchar_t";
            symbol.kind = SymbolKind::Macro;

            index8.add_relation(symbol,
                                Relation{
                                    .kind = RelationKind::Reference,
                                    .range = {4100, 4114},
                                    .target_symbol = 0ull,
            });
        }
        {
            auto& symbol = index8.get_symbol(12199573421319547529ull);
            symbol.name = "__need_size_t";
            symbol.kind = SymbolKind::Macro;

            index8.add_relation(symbol,
                                Relation{
                                    .kind = RelationKind::Reference,
                                    .range = {3867, 3880},
                                    .target_symbol = 0ull,
            });
        }
        {
            auto& symbol = index8.get_symbol(6389328935281374692ull);
            symbol.name = "__need_NULL";
            symbol.kind = SymbolKind::Macro;

            index8.add_relation(symbol,
                                Relation{
                                    .kind = RelationKind::Reference,
                                    .range = {4212, 4223},
                                    .target_symbol = 0ull,
            });
        }

        RawIndex index9;
        {
            auto& symbol = index9.get_symbol(6389328935281374692ull);
            symbol.name = "__need_NULL";
            symbol.kind = SymbolKind::Macro;

            index9.add_relation(symbol,
                                Relation{
                                    .kind = RelationKind::Reference,
                                    .range = {4212, 4223},
                                    .target_symbol = 0ull,
            });
        }

        RawIndex index10;
        {
            auto& symbol = index10.get_symbol(12199573421319547529ull);
            symbol.name = "__need_size_t";
            symbol.kind = SymbolKind::Macro;

            index10.add_relation(symbol,
                                 Relation{
                                     .kind = RelationKind::Reference,
                                     .range = {3867, 3880},
                                     .target_symbol = 0ull,
            });
        }

        {
            HeaderIndex base;
            auto context = base.merge("main.cpp", 56, index1);
            expect(refl::equal(context, HeaderIndex::HeaderContext{56, 0, 0}));

            context = base.merge("main.cpp", 83, index2);
            expect(refl::equal(context, HeaderIndex::HeaderContext{83, 1, 1}));

            context = base.merge("main.cpp", 87, index3);
            expect(refl::equal(context, HeaderIndex::HeaderContext{87, 2, 2}));

            context = base.merge("main.cpp", 118, index4);
            expect(refl::equal(context, HeaderIndex::HeaderContext{118, 3, 1}));

            context = base.merge("main.cpp", 135, index5);
            expect(refl::equal(context, HeaderIndex::HeaderContext{135, 4, 3}));

            context = base.merge("main.cpp", 147, index6);
            expect(refl::equal(context, HeaderIndex::HeaderContext{147, 5, 1}));

            context = base.merge("main.cpp", 150, index7);
            expect(refl::equal(context, HeaderIndex::HeaderContext{150, 6, 0}));

            context = base.merge("main.cpp", 178, index8);
            expect(refl::equal(context, HeaderIndex::HeaderContext{178, 7, 2}));

            context = base.merge("main.cpp", 212, index9);
            expect(refl::equal(context, HeaderIndex::HeaderContext{212, 8, 4}));

            context = base.merge("main.cpp", 226, index10);
            expect(refl::equal(context, HeaderIndex::HeaderContext{226, 9, 3}));
        }
    };
};

#if 0

#endif
}  // namespace

}  // namespace clice::testing
