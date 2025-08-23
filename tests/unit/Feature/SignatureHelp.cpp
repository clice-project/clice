#include "Test/Tester.h"
#include "Feature/SignatureHelp.h"

namespace clice::testing {

namespace {

suite<"SignatureHelp"> signature_help = [] {
    Tester tester;
    proto::SignatureHelp help;

    auto run = [&](llvm::StringRef code) {
        tester.clear();
        tester.add_main("main.cpp", code);
        tester.prepare();

        tester.params.completion = {"main.cpp", tester.nameless_points()[0]};

        help = feature::signature_help(tester.params, {});
    };

    test("Simple") = [&] {
        run(R"cpp(
void foo();

void foo(int x);

void foo(int x, int y);

int main() {
    foo($);
}
)cpp");

        expect(eq(help.signatures.size(), 3));
    };

    /// FIXME: Add more tests.
};

}  // namespace

}  // namespace clice::testing

