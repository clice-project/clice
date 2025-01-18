#include "Test/Test.h"
#include "Basic/SourceConverter.h"
#include "Compiler/Preamble.h"
#include "Support/Format.h"

namespace clice::testing {

namespace {

TEST(Preamble, ComputeBounds) {
    proto::Position pos;
    SourceConverter converter;

    Annotation annotation = {"\n\n\nint x = 1;"};
    auto compute = [&](llvm::StringRef source) {
        annotation = {source};
        auto content = annotation.source();
        return converter.toPosition(content, computeBounds(content));
    };

    pos = compute("#include <iostream>$(end)");
    EXPECT_EQ(pos, annotation.pos("end"));

    pos = compute("#include <iostream>$(end)\n");
    EXPECT_EQ(pos, annotation.pos("end"));

    pos = compute(R"cpp(
#ifdef TEST
#include <iostream>
#define 1
#endif$(end)
    )cpp");
    EXPECT_EQ(pos, annotation.pos("end"));

    pos = compute(R"cpp(
#include <iostream>$(end)
int x = 1;
    )cpp");
    EXPECT_EQ(pos, annotation.pos("end"));

    pos = compute(R"cpp(
module;
#include <iostream>$(end)
export module test;
    )cpp");
    EXPECT_EQ(pos, annotation.pos("end"));
}

}  // namespace
}  // namespace clice::testing

