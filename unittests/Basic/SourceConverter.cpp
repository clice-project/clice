#include "Test/Test.h"
#include "Basic/SourceConverter.h"

namespace clice {

namespace {

TEST(SourceConverter, Remeasure) {
    using SC = SourceConverter;

    SourceConverter utf8{proto::PositionEncodingKind::UTF8};

    EXPECT_EQ(utf8.remeasure(""), 0);
    EXPECT_EQ(utf8.remeasure("ascii"), 5);

    SourceConverter utf16{proto::PositionEncodingKind::UTF16};

    EXPECT_EQ(utf16.remeasure("↓"), 1);
    EXPECT_EQ(utf16.remeasure("¥"), 1);

    SourceConverter utf32{proto::PositionEncodingKind::UTF32};
    EXPECT_EQ(utf8.remeasure("😂"), 4);
    EXPECT_EQ(utf16.remeasure("😂"), 2);
    EXPECT_EQ(utf32.remeasure("😂"), 1);
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
#if defined(__unix__)
    const char* paths[] = {"/dev/null"};
    const char* uris[] = {"file:///dev/null"};
#elif defined(_WIN32)
    const char* paths[] = {"C:\\Windows\\System32\\notepad.exe"};
    const char* uris[] = {"file:///C:/Windows/System32/notepad.exe"};
#else
#error "Unsupported platform"
#endif

    EXPECT_EQ(std::size(paths), std::size(uris));

    for(int i = 0; i < std::size(paths); ++i) {
        EXPECT_EQ(SC::toURI(paths[i]), uris[i]);
        EXPECT_EQ(paths[i], SC::toPath(uris[i]));
    }
}

}  // namespace

}  // namespace clice

