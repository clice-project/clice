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

TEST(SourceConverter, UriAndFsPath) {
    using SC = SourceConverter;

    // It must be a existed file.
#ifdef unix
    const char* fspath[] = {"/dev/null"};
    const char* uri[] = {"file:///dev/null"};
#else
    const char* fspath[] = {};
    const char* uri[] = {};
#endif
    EXPECT_EQ(std::size(fspath), std::size(uri));

    for(int i = 0; i < std::size(fspath); ++i) {
        auto fspath_ = fspath[i];
        auto uri_ = uri[i];

        auto uri1 = SC::toUri(fspath_);
        EXPECT_TRUE(bool(uri1));
        EXPECT_EQ(*uri1, uri_);

        auto uri2 = SC::toUriUnchecked(fspath_);
        EXPECT_EQ(uri2, uri_);

        auto fspath1 = SC::toRealPathUnchecked(uri_);
        EXPECT_EQ(fspath1, fspath_);

        auto fspath2 = SC::toRealPath(uri_);
        EXPECT_TRUE(bool(fspath2));
        EXPECT_EQ(*fspath2, fspath_);
    }
}

}  // namespace

}  // namespace clice

