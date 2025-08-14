#include "Test/Test.h"
#include "AST/SourceCode.h"

namespace clice::testing {

suite<"SourceCode"> source_code = [] {
    test("IgnoreComments") = [] {
        /// Test all tokens.
        std::size_t count = 0;

        std::vector<clang::tok::TokenKind> kinds = {
            clang::tok::raw_identifier,
            clang::tok::raw_identifier,
            clang::tok::equal,
            clang::tok::numeric_constant,
            clang::tok::semi,
        };

        {
            /// Test ignore comments.
            Lexer lexer("int x = 1; // comment", true);

            while(true) {
                Token token = lexer.advance();
                if(token.is_eof()) {
                    break;
                }

                expect(that % token.kind == kinds[count]);
                count += 1;
            }

            expect(that % count == 5);
        }

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

        {
            /// Test retain comments.
            Lexer lexer("int x = 1; // comment", false);

            while(true) {
                Token token = lexer.advance();
                if(token.is_eof()) {
                    break;
                }

                expect(that % token.kind == kinds[count]);
                count += 1;
            }

            expect(that % count == 6);
        }
    };

    test("LexInclude") = [] {
        /// TODO: test eod
        /// test multiple lines macros.

        Lexer lexer(R"(
#include <iostream>
#include "gtest/test.h"
module;
int x = 1;
)",
                    true,
                    nullptr,
                    false);

        // while(true) {
        //     Token token = lexer.advance();
        //     if(token.is_eof()) {
        //         break;
        //     }
        //
        //    println("kind: {}", token.name());
        //}
    };
};

}  // namespace clice::testing
