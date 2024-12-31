#include "../Test.h"
#include "Basic/SourceConverter.h"

namespace clice {

namespace {

TEST(SourceCode, Remeasure) {
    using SC = SourceConverter;

    EXPECT_EQ(SC::remeasure("", proto::PositionEncodingKind::UTF8), 0);
    EXPECT_EQ(SC::remeasure("ascii", proto::PositionEncodingKind::UTF8), 5);

    EXPECT_EQ(SC::remeasure("↓", proto::PositionEncodingKind::UTF16), 1);
    EXPECT_EQ(SC::remeasure("¥", proto::PositionEncodingKind::UTF16), 1);

    EXPECT_EQ(SC::remeasure("😂", proto::PositionEncodingKind::UTF16), 2);
    EXPECT_EQ(SC::remeasure("😂", proto::PositionEncodingKind::UTF32), 1);
}

}  // namespace

}  // namespace clice

