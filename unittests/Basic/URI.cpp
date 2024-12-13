#include <gtest/gtest.h>
#include <Basic/URI.h>

namespace clice {

namespace {

TEST(URI, Basic) {
    URI uri("https", "reviews.llvm.org", "/D41946");

    EXPECT_EQ(uri.scheme(), "https");
    EXPECT_EQ(uri.authority(), "reviews.llvm.org");
    EXPECT_EQ(uri.body(), "/D41946");
}

TEST(URI, Copy) {
    URI uri1("https", "reviews.llvm.org", "/D41946");
    URI uri2(uri1);

    EXPECT_EQ(uri2.scheme(), "https");
    EXPECT_EQ(uri2.authority(), "reviews.llvm.org");
    EXPECT_EQ(uri2.body(), "/D41946");
}

TEST(URI, Eq) {
    URI uri1("https", "reviews.llvm.org", "/D41946");
    URI uri2("https", "reviews.llvm.org", "/D41946");
    URI uri3("http", "example.com", "/index.html");

    EXPECT_TRUE(uri1 == uri2);
    EXPECT_FALSE(uri1 == uri3);
}

TEST(URI, File) {
    auto uri = URI::from("/home/user/file.txt");
    EXPECT_EQ(uri.scheme(), "file");
    EXPECT_EQ(uri.authority(), "");
    EXPECT_EQ(uri.body(), "/home/user/file.txt");
    EXPECT_EQ(uri.toString(), "file:///home/user/file.txt");
}

TEST(URI, Parse) {
    auto expectedUri = URI::parse("https://reviews.llvm.org/D41946");
    ASSERT_TRUE(bool(expectedUri));

    URI& uri = expectedUri.get();
    EXPECT_EQ(uri.scheme(), "https");
    EXPECT_EQ(uri.authority(), "reviews.llvm.org");
    EXPECT_EQ(uri.body(), "/D41946");
}

}  // namespace

}  // namespace clice

