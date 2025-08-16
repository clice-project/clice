#include "Test/Test.h"

namespace clice::testing {

namespace {

suite<"TEST.Example"> suite = [] {
    skip / test("Skipped") = [] {
        expect(false);
    };

    test("Simple") = [] {
        expect(false);
    };

    test("Expression") = [] {
        expect(eq(1, 2));
        expect(ne(1, 1));
        expect(eq(add(1, 2), 4));
        expect(lt(2, 1));
        expect(lt(sub(4, 1), 1));

        /// expect(has(std::vector{1, 2, 3}, 4));
    };

    test("Expression") = [] {
        std::vector v{1, 2, 3};
        fatal / expect(eq(v.size(), 4));
        expect(eq(v[3], 1));
        /// expect(has(std::vector{1, 2, 3}, 4));
    };
};

}  // namespace

}  // namespace clice::testing
