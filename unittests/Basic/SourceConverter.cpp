#include "Test/CTest.h"
#include "Basic/SourceConverter.h"

namespace clice::testing {

namespace {

TEST(SourceConverter, Remeasure) {
    using SC = SourceConverter;

    SourceConverter utf8{proto::PositionEncodingKind::UTF8};

    EXPECT_EQ(utf8.remeasure(""), 0);
    EXPECT_EQ(utf8.remeasure("ascii"), 5);

    SourceConverter utf16{proto::PositionEncodingKind::UTF16};

#ifndef _MSC_VER
    // MSVC can not recognize the character.
    EXPECT_EQ(utf16.remeasure("↓"), 1);
#endif 
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

    auto& src = txs.info->srcMgr();
    auto& tks = txs.info->tokBuf();

    auto mainid = src.getMainFileID();
    auto tokens =
        tks.expandedTokens({src.getLocForStartOfFile(mainid), src.getLocForEndOfFile(mainid)});

    auto eof = tokens.back().endLocation();

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
        EXPECT_EQ(pos.character, 17);
    }

    {
        SourceConverter cvtr{proto::PositionEncodingKind::UTF32};
        auto pos = cvtr.toPosition(eof, src);
        EXPECT_EQ(pos.line, 0);
        EXPECT_EQ(pos.character, 16);
    }
}

TEST(SourceConverter, LocalRangeAndPosition) {
    const char* main = "$(begin)int a$(mid) /*😂*/ = 1;$(eof)";

    Tester txs("main.cpp", main);
    txs.run();

    SourceConverter cvtr{proto::PositionEncodingKind::UTF8};

    {
        auto begOff = txs.offset("begin");
        EXPECT_EQ(begOff, 0);

        auto beginPos = cvtr.toPosition(main, begOff);
        EXPECT_EQ(beginPos, txs.pos("begin"));
        EXPECT_EQ(begOff, cvtr.toOffset(main, beginPos));
    }

    {
        auto midOff = txs.offset("mid");
        auto midPos = cvtr.toPosition(main, midOff);
        EXPECT_EQ(midPos, txs.pos("mid"));
        EXPECT_EQ(midOff, cvtr.toOffset(main, midPos));
    }

    {
        auto eofOff = txs.offset("eof");
        auto eofPos = cvtr.toPosition(main, eofOff);
        EXPECT_EQ(eofPos, txs.pos("eof"));
        EXPECT_EQ(eofOff, cvtr.toOffset(main, eofPos));
    }
}

TEST(SourceConverter, UriAndFsPath) {
    using SC = SourceConverter;

    // It must be a existed file.
#if defined(__unix__)
    llvm::StringRef paths[] = {"/dev/null"};
    llvm::StringRef uris[] = {"file:///dev/null"};
#elif defined(_WIN32)
    llvm::StringRef paths[] = {"C:\\Windows\\System32\\notepad.exe"};
    llvm::StringRef uris[] = {"file:///C%3A/Windows/System32/notepad.exe"};
#else
#error "Unsupported platform"
#endif

    EXPECT_EQ(std::size(paths), std::size(uris));

    for(int i = 0; i < std::size(paths); ++i) {
        llvm::outs() << ": " << SC::toURI(paths[i]) << "\n";
        llvm::outs() << ": " << SC::toPath(uris[i]) << "\n";
        EXPECT_EQ(SC::toURI(paths[i]), uris[i]);
        EXPECT_EQ(paths[i], SC::toPath(uris[i]));
    }
}

}  // namespace

}  // namespace clice::testing
