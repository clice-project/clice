#include "Tester.h"

namespace clice {

namespace {

TEST(Index, Annotation) {
    const char* code = R"cpp(
int @name = 1;

int main() {
    $(d1)name = 2;
}
)cpp";

    IndexerTester tester(code);
    tester.GotoDefinition("d1", "name");
}

}  // namespace

}  // namespace clice

