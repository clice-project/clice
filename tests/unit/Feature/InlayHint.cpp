#include "Test/Tester.h"
#include "Feature/InlayHint.h"

namespace clice::testing {

namespace {

suite<"InlayHint"> inlay_hint = [] {
    Tester tester;
    feature::InlayHints hints;

    auto run = [&](llvm::StringRef code, llvm::StringRef name) {
        tester.clear();
        tester.add_main("main.cpp", code);
        tester.compile();

        LocalSourceRange range = LocalSourceRange(0, tester.unit->interested_content().size());
        hints = feature::inlay_hints(*tester.unit, range);

        expect(that % tester.nameless_points().size() == 1);

        /// bool visited = false;
        /// for(auto& hint: hints) {
        ///     if(hint.offset == tester.nameless_points().front() && hint.parts.front().name ==
        ///     name){
        ///         visited = true;
        ///         break;
        ///     }
        /// }
        /// expect(that % visited);
    };

    test("Parameters") = [&] {
        run(R"cpp(
void foo(int param);
void bar() {
  foo($42);
}
)cpp",
            "param");
    };
};

}  // namespace
}  // namespace clice::testing
