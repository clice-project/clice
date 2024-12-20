#include "../Test.h"
#include "Basic/SourceCode.h"

namespace clice {

namespace {

TEST(SourceCode, Remeasure) {
    EXPECT_EQ(remeasure("", proto::PositionEncodingKind::UTF8), 0);
    EXPECT_EQ(remeasure("ascii", proto::PositionEncodingKind::UTF8), 5);

    EXPECT_EQ(remeasure("↓", proto::PositionEncodingKind::UTF16), 1);
    EXPECT_EQ(remeasure("¥", proto::PositionEncodingKind::UTF16), 1);

    EXPECT_EQ(remeasure("😂", proto::PositionEncodingKind::UTF16), 2);
    EXPECT_EQ(remeasure("😂", proto::PositionEncodingKind::UTF32), 1);
}

}  // namespace

}  // namespace clice

