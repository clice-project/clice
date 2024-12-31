#include "../Test.h"
#include "Basic/SourceConverter.h"

namespace clice {

namespace {

TEST(SourceConverter, Remeasure) {
    using SC = SourceConverter;

    EXPECT_EQ(SC::remeasure("", proto::PositionEncodingKind::UTF8), 0);
    EXPECT_EQ(SC::remeasure("ascii", proto::PositionEncodingKind::UTF8), 5);

    EXPECT_EQ(SC::remeasure("↓", proto::PositionEncodingKind::UTF16), 1);
    EXPECT_EQ(SC::remeasure("¥", proto::PositionEncodingKind::UTF16), 1);

    EXPECT_EQ(SC::remeasure("😂", proto::PositionEncodingKind::UTF8), 4);
    EXPECT_EQ(SC::remeasure("😂", proto::PositionEncodingKind::UTF16), 2);
    EXPECT_EQ(SC::remeasure("😂", proto::PositionEncodingKind::UTF32), 1);
}

TEST(SourceConverter, Position) {
    const char* main = "int a /*😂*/ = 1;$(eof)";

    Tester txs("main.cpp", main);
    txs.run("-std=c++11");

    auto& src = txs.info.srcMgr();
    auto& tks = txs.info.tokBuf();

    auto mainid = src.getMainFileID();
    auto tokens =
        tks.expandedTokens({src.getLocForStartOfFile(mainid), src.getLocForEndOfFile(mainid)});

    auto eof = tokens.back().endLocation();
    txs.expect("eof", eof);

    {
        SourceConverter cvtr{proto::PositionEncodingKind::UTF8};
        auto pos = cvtr.toPosition(eof, src);
        EXPECT_EQ(pos.line, 0);
        EXPECT_EQ(pos.character, 19);
    }

    {
        SourceConverter cvtr{proto::PositionEncodingKind::UTF16};
        auto pos = cvtr.toPosition(eof, src);
        EXPECT_EQ(pos.line, 0);
        EXPECT_EQ(pos.character, 19);
    }

    {
        SourceConverter cvtr{proto::PositionEncodingKind::UTF32};
        auto pos = cvtr.toPosition(eof, src);
        EXPECT_EQ(pos.line, 0);
        EXPECT_EQ(pos.character, 19);
    }
}

}  // namespace

}  // namespace clice

