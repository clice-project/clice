#include "Test/CTest.h"
#include "Index/SymbolIndex.h"

namespace clice::testing {

struct SymbolIndex : ::testing::Test, Tester {
    index::Shared<std::vector<char>> indices;
    index::SymbolIndex index = {nullptr, 0};

    void run(llvm::StringRef code) {
        addMain("main.cpp", code);
        Tester::run();
        indices = index::SymbolIndex::build(*info);
        index = {
            indices[info->getInterestedFile()].data(),
            static_cast<uint32_t>(indices[info->getInterestedFile()].size()),
        };
    }

    void EXPECT_OCCURENCE(int index,
                          llvm::StringRef pos,
                          llvm::StringRef name,
                          std::source_location location = std::source_location::current()) {
        auto occurrence = this->index.occurrences()[index];
        EXPECT_EQ(occurrence.range().begin, this->offset(pos), location);
        EXPECT_EQ(occurrence.symbol().name(), name, location);
    }

    index::Symbol EXPECT_SYMBOL(llvm::StringRef name,
                                SymbolKind kind,
                                std::source_location location = std::source_location::current()) {
        for(auto symbol: index.symbols()) {
            if(symbol.name() == name) {
                EXPECT_EQ(symbol.kind(), kind, location);
                return symbol;
            }
        }

        EXPECT_FAILURE("can not find symbol", location);
        std::unreachable();
    }

    void EXPECT_RELATION(llvm::StringRef name,
                         RelationKind kind,
                         llvm::StringRef pos,
                         std::source_location location = std::source_location::current()) {
        bool foundSymbol = false;
        for(auto symbol: index.symbols()) {
            if(symbol.name() == name) {
                foundSymbol = true;

                bool foundRelation = false;
                for(auto relation: symbol.relations()) {
                    if((relation.kind() & kind) && relation.range().begin == offset(pos)) {
                        foundRelation = true;
                        break;
                    }
                }
                foundRelation = true;
                EXPECT_EQ(foundRelation, true, location);

                break;
            }
        }
        EXPECT_EQ(foundSymbol, true, location);
    }
};

namespace {

TEST_F(SymbolIndex, Symbols) {
    llvm::StringRef code = R"(
int x = 1;
int y = 2;

struct Foo {};
)";

    run(code);

    EXPECT_EQ(index.symbols().length(), 3);
    EXPECT_EQ(index.path(), "main.cpp");
    EXPECT_EQ(index.content(), code);
}

TEST_F(SymbolIndex, Occurrences) {
    llvm::StringRef code = R"(
int @x = 1;
int @y = 2;

struct @Foo {};
)";
    run(code);

    EXPECT_EQ(index.occurrences().length(), 3);
    EXPECT_EQ(index.path(), "main.cpp");

    /// FIXME: Use StringMap to store sources.
    EXPECT_EQ(index.content(), sources[0]);

    EXPECT_OCCURENCE(0, "x", "x");
    EXPECT_OCCURENCE(1, "y", "y");
    EXPECT_OCCURENCE(2, "Foo", "Foo");
}

TEST_F(SymbolIndex, Relations) {
    llvm::StringRef code = R"(
int $(1)x = 1;

int main() {
    $(2)x = 2;
}
)";
    run(code);

    EXPECT_SYMBOL("x", SymbolKind::Variable);
    EXPECT_RELATION("x", RelationKind::Definition, "1");
    EXPECT_RELATION("x", RelationKind::Reference, "2");
}

TEST_F(SymbolIndex, LocateSymbol) {
    llvm::StringRef code = R"(
int $(1)x = 1;

int main() {
    $(2)x = 2;
}
)";
    run(code);

    auto symbols = index.locateSymbol(offset("1"));
    EXPECT_EQ(symbols.size(), 1);
    EXPECT_EQ(symbols[0].name(), "x");

    symbols = index.locateSymbol(offset("2"));
    EXPECT_EQ(symbols.size(), 1);
    EXPECT_EQ(symbols[0].name(), "x");

    auto symbol = index.locateSymbol(symbols[0].id());
    EXPECT_EQ(symbol.has_value(), true);
    EXPECT_EQ(symbols[0].name(), "x");
}

TEST_F(SymbolIndex, JSON) {
    llvm::StringRef code = R"(
#include <stddef.h>
)";
    run(code);

    for(auto& [fid, index]: indices) {
        if(index.size() == 0) {
            continue;
        }

        index::SymbolIndex sindex(index.data(), index.size());
        auto json = sindex.toJSON();
    }
}

}  // namespace

}  // namespace clice::testing
