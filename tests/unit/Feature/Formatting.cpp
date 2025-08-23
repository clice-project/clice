#include "Test/Test.h"
#include "Feature/Formatting.h"

namespace clice::testing {

namespace {

suite<"Formatting"> suite = [] {
    test("Simple") = [] {
        auto edits = feature::document_format("main.cpp", "int main() { return 0; }", std::nullopt);
        expect(ne(edits.size(), 0));
    };
};

}  // namespace

}  // namespace clice::testing
