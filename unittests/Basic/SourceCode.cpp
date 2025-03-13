#include "Test/Test.h"
#include "Basic/SourceCode.h"

namespace clice::testing {

TEST(SourceCode, tokenize) {
    std::size_t count = 0;

    /// Test breaking the loop.
    tokenize("int x = 1;", [&](const clang::Token& token) {
        count++;
        return false;
    });

    EXPECT_EQ(count, 1);

    /// Test all tokens.
    count = 0;

    std::vector<clang::tok::TokenKind> kinds = {
        clang::tok::raw_identifier,
        clang::tok::raw_identifier,
        clang::tok::equal,
        clang::tok::numeric_constant,
        clang::tok::semi,
    };

    tokenize("int x = 1; // comment", [&](const clang::Token& token) {
        if(token.is(clang::tok::eof)) [[unlikely]] {
            return false;
        }

        EXPECT_EQ(token.getKind(), kinds[count]);
        count++;
        return true;
    });

    EXPECT_EQ(count, 5);

    /// Test retain comments.
    count = 0;

    kinds = {
        clang::tok::raw_identifier,
        clang::tok::raw_identifier,
        clang::tok::equal,
        clang::tok::numeric_constant,
        clang::tok::semi,
        clang::tok::comment,
    };

    tokenize(
        "int x = 1; /// comment",
        [&](const clang::Token& token) {
            if(token.is(clang::tok::eof)) [[unlikely]] {
                return false;
            }

            EXPECT_EQ(token.getKind(), kinds[count]);
            count++;
            return true;
        },
        false);

    EXPECT_EQ(count, 6);
}

}  // namespace clice::testing
