#include "Test/Test.h"
#include "Compiler/Diagnostic.h"
#include "Compiler/Compilation.h"

namespace clice::testing {

namespace {

using namespace clice;

TEST(Diagnostic, CommandError) {
    CompilationParams params;
    /// miss input file.
    params.arguments = {"clang++"};
    params.add_remapped_file("main.cpp", "int main() { return 0; }");
    auto unit = compile(params);
    ASSERT_FALSE(unit);
}

TEST(Diagnostic, Error) {
    CompilationParams params;
    params.arguments = {"clang++", "main.cpp"};
    params.add_remapped_file("main.cpp", "int main() { return 0 }");
    auto unit = compile(params);
    ASSERT_TRUE(unit);
    ASSERT_TRUE(!unit->diagnostics().empty());

    for(auto& diag: unit->diagnostics()) {
        clice::println("{}", diag.message);
    }
}

}  // namespace

}  // namespace clice::testing

