#include "Test/Tester.h"
#include "Feature/InlayHint.h"

namespace clice::testing {

namespace {

struct InlayHints : public Tester, ::testing::Test {
protected:
    feature::InlayHints hints;

    void run(llvm::StringRef code, llvm::StringRef name) {
        add_main("main.cpp", code);
        compile();

        LocalSourceRange range = LocalSourceRange(0, unit->interested_content().size());
        hints = feature::inlay_hints(*unit, range);

        assert(nameless_points().size() == 1);

        bool visited = false;
        for(auto& hint: hints) {
            if(hint.offset == nameless_points().front() && hint.parts.front().name == name){
                visited = true;
                break;
            }
        }
        EXPECT_TRUE(visited);
    }
};

TEST_F(InlayHints, Parameters) {
    run(R"cpp(
void foo(int param);
void bar() {
  foo($42);
}
)cpp",
        "param");
}

}  // namespace
}  // namespace clice::testing
