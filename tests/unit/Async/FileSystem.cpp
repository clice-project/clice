#include "Test/Test.h"
#include "Async/Async.h"
#include "Support/FileSystem.h"

namespace clice::testing {

namespace {

TEST(Async, FileSystemRead) {
    auto path = fs::createTemporaryFile("prefix", "suffix");
    EXPECT_TRUE(path.has_value());

    auto result = fs::write(*path, "hello");
    EXPECT_TRUE(result.has_value());

    auto main = [&] -> async::Task<> {
        auto content = co_await async::fs::read(*path);
        EXPECT_TRUE(content.has_value());
        EXPECT_EQ(*content, "hello");
    };

    async::run(main());
}

TEST(Async, FileSystemWrite) {
    auto path = fs::createTemporaryFile("prefix", "suffix");
    EXPECT_TRUE(path.has_value());

    auto main = [&] -> async::Task<> {
        char buffer[] = "hello";

        auto result = co_await async::fs::write(*path, buffer, 5);
        EXPECT_TRUE(result.has_value());
    };

    async::run(main());

    auto content = fs::read(*path);
    EXPECT_TRUE(content.has_value());
    EXPECT_EQ(*content, "hello");
}

}  // namespace

}  // namespace clice::testing

