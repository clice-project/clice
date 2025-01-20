#include "Test/IndexTester.h"

namespace clice::testing {

namespace {

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

    auto json = index.toJSON();

    {
        std::error_code ec;
        llvm::raw_fd_ostream file("index.sidx", ec);
        ASSERT_FALSE(ec);
        file.write(static_cast<char*>(index.base), index.size);
    }

    {
        auto file = llvm::MemoryBuffer::getFile("index.sidx");
        ASSERT_TRUE(bool(file));
        auto& buffer = file.get();
        auto size = buffer->getBufferSize();
        auto base = std::malloc(size);
        std::memcpy(base, buffer->getBufferStart(), size);
        index::SymbolIndex index(base, size);
        EXPECT_EQ(json, index.toJSON());
    }
}

}  // namespace

}  // namespace clice::testing

