#include "Test/Test.h"
#include "Server/Tidy.h"

namespace clice::testing {

namespace {

suite<"ClangTidy"> clang_tidy = [] {
    test("isFastTidyCheck") = [] {
        expect(that % tidy::isFastTidyCheck("readability-misleading-indentation"));
        expect(that % tidy::isFastTidyCheck("bugprone-unused-return-value"));

        // clangd/unittests/TidyProviderTests.cpp
        expect(that % tidy::isFastTidyCheck("misc-const-correctness") == false);
        expect(that % tidy::isFastTidyCheck("bugprone-suspicious-include") == true);
        expect(that % tidy::isFastTidyCheck("replay-preamble-check") == std::nullopt);
    };
};

}  // namespace
}  // namespace clice::testing
