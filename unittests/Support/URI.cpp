#include <gtest/gtest.h>
#include <Support/URI.h>

namespace {

using namespace clice;

TEST(URITest, ConstructorAndAccessors) {
    URI uri("https", "reviews.llvm.org", "/D41946");

    EXPECT_EQ(uri.scheme(), "https");
    EXPECT_EQ(uri.authority(), "reviews.llvm.org");
    EXPECT_EQ(uri.body(), "/D41946");
}

TEST(URITest, CopyConstructor) {
    URI uri1("https", "reviews.llvm.org", "/D41946");
    URI uri2(uri1);

    EXPECT_EQ(uri2.scheme(), "https");
    EXPECT_EQ(uri2.authority(), "reviews.llvm.org");
    EXPECT_EQ(uri2.body(), "/D41946");
}

TEST(URITest, EqualityOperator) {
    URI uri1("https", "reviews.llvm.org", "/D41946");
    URI uri2("https", "reviews.llvm.org", "/D41946");
    URI uri3("http", "example.com", "/index.html");

    EXPECT_TRUE(uri1 == uri2);
    EXPECT_FALSE(uri1 == uri3);
}

TEST(URITest, ParseFunction) {
    auto expectedUri = URI::parse("https://reviews.llvm.org/D41946");
    ASSERT_TRUE(static_cast<bool>(expectedUri));

    URI uri = expectedUri.get();
    EXPECT_EQ(uri.scheme(), "https");
    EXPECT_EQ(uri.authority(), "reviews.llvm.org");
    EXPECT_EQ(uri.body(), "/D41946");
}

}  // namespace

