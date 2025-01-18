#include "Test/Test.h"
#include "Basic/SourceConverter.h"
#include "Compiler/Preamble.h"

namespace clice::testing {

namespace {

TEST(Preamble, ComputeBounds) {
    const char* normal = R"cpp(
#ifdef TEST
#include <iostream>
#define 1
#endif
    )cpp";

    SourceConverter converter;

    llvm::outs() << computeBounds(normal) << '\n';
}

}  // namespace

}  // namespace clice::testing

