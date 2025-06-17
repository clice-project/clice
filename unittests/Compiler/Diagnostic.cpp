#include "Test/Test.h"
#include "Compiler/Diagnostic.h"
#include "Compiler/Compilation.h"

namespace clice::testing {

namespace {

using namespace clice;

TEST(Diagnostic, Error) {
    CompilationParams params;
    params.command = "clang++ main.cpp";
    params.add_remapped_file("main.cpp", "int main() { return 0; }");
    auto unit = compile(params);
    /// ASSERT_FALSE(unit);
    /// clice::println("{}", unit.error());

    clice::println("{}", unit->file_content(unit->interested_file()));
}

}  // namespace

}  // namespace clice::testing

