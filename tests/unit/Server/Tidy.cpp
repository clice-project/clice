#include "Test/Test.h"
#include "Server/Tidy.h"

namespace clice::testing {

namespace {

suite<"ClangTidy"> clang_tidy = [] {
    test("isFastTidyCheck") = [] {
        expect(that % tidy::isFastTidyCheck("readability-misleading-indentation"));
        expect(that % tidy::isFastTidyCheck("bugprone-unused-return-value"));
    };
};

}  // namespace
}  // namespace clice::testing
