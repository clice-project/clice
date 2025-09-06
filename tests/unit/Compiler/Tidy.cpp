#include "Test/Test.h"
#include "Compiler/Tidy.h"

namespace clice::testing {

namespace {

suite<"ClangTidy"> clang_tidy = [] {
    test("is_fast_tidy_check") = [] {
        expect(that % tidy::is_fast_tidy_check("readability-misleading-indentation"));
        expect(that % tidy::is_fast_tidy_check("bugprone-unused-return-value"));

        // clangd/unittests/TidyProviderTests.cpp
        expect(that % tidy::is_fast_tidy_check("misc-const-correctness") == false);
        expect(that % tidy::is_fast_tidy_check("bugprone-suspicious-include") == true);
        expect(that % tidy::is_fast_tidy_check("replay-preamble-check") == std::nullopt);
    };
};

}  // namespace
}  // namespace clice::testing
