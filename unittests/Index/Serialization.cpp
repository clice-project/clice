#include "Test/IndexTester.h"

namespace clice::testing {

namespace {

TEST(Index, locateSymbol) {
    const char* code = R"cpp(
void $(1)foo();

void $(2)foo() {}
)cpp";

    IndexTester tester{"main.cpp", code};
    tester.run();
    auto& info = *tester.info;
    auto indices = index::index(info);

    ASSERT_EQ(indices.size(), 1);
    auto& index = indices.begin()->second;

    llvm::SmallVector<index::SymbolIndex::Symbol> symbols;
    /// index.locateSymbols(tester.offsets["1"], symbols);
    ASSERT_EQ(symbols.size(), 1);
    auto symbol = symbols[0];
}

TEST(Index, Serialization) {
    const char* code = R"cpp(
void $(1)foo();

void $(2)foo() {}
)cpp";

    IndexTester tester{"main.cpp", code};
    tester.run();
    auto& info = *tester.info;
    auto indices = index::index(info);

    ASSERT_EQ(indices.size(), 1);
    auto& index = indices.begin()->second;

    /// auto json = index.toJSON();

    // auto path = fs::createTemporaryFile("index", ".sidx");
    // ASSERT_TRUE(path.has_value());
    //
    //{
    //    std::error_code ec;
    //    llvm::raw_fd_ostream file(*path, ec);
    //    ASSERT_FALSE(ec);
    //    file.write(static_cast<char*>(index.base), index.size);
    //}
    //
    //{
    //    auto file = llvm::MemoryBuffer::getFile(*path);
    //    ASSERT_TRUE(bool(file));
    //    auto& buffer = file.get();
    //    auto size = buffer->getBufferSize();
    //    auto base = static_cast<char*>(std::malloc(size));
    //    std::memcpy(base, buffer->getBufferStart(), size);
    //    index::SymbolIndex index(base, size);
    //    EXPECT_EQ(json, index.toJSON());
    //}
}

}  // namespace

}  // namespace clice::testing

