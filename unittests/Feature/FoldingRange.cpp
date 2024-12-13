#include <gtest/gtest.h>
#include <Feature/FoldingRange.h>

#include "../Test.h"

namespace clice {

namespace {

void dbg(const proto::FoldingRangeResult& result) {
    for(auto& item: result) {
        llvm::outs()
            << std::format("begin/end line: {}/{},  begin/end character: {}/{}, kind: {}, text: {}",
                           item.startLine,
                           item.endLine,
                           item.startCharacter,
                           item.endCharacter,
                           json::serialize(item.kind),
                           item.collapsedText)
            << "\n";
    }
}

TEST(FoldingRange, Namespace) {
    const char* main = R"cpp(
namespace single_line {$(1)
//
}$(2)

namespace with_nodes {$(3)
//
//struct _ {};
}$(4)

namespace empty {}

namespace ugly 

{$(5)
//
//
}$(6)

)cpp";

    Tester txs("main.cpp", main);
    txs.run();

    auto& info = txs.info;
    auto toLoc = [srcMgr = &info.srcMgr()](uint lsp_ln, uint lsp_col) -> clang::SourceLocation {
        auto fileID = srcMgr->getMainFileID();
        return srcMgr->translateLineCol(fileID, lsp_ln + 2, lsp_col + 2);
    };

    FoldingRangeParams param;
    auto res = feature::foldingRange(param, info);

    // dbg(res);

    txs.equal(res.size(), 3)
        //
        .expect("1", toLoc(res[0].startLine, res[0].startCharacter))
        .expect("2", toLoc(res[0].endLine, res[0].endCharacter))
        //
        .expect("3", toLoc(res[1].startLine, res[1].startCharacter))
        .expect("4", toLoc(res[1].endLine, res[1].endCharacter))
        //
        .expect("5", toLoc(res[2].startLine, res[2].startCharacter))
        .expect("6", toLoc(res[2].endLine, res[2].endCharacter))
        //
        ;
}

TEST(FoldingRange, Enum) {
    auto main = R"cpp(
enum _0 {$(1)
    A,
    B,
    C
};$(2)

enum _1 { D };

enum class _2 {$(3)
    A,
    B,
    C
};$(4)

)cpp";

    Tester txs("main.cpp", main);
    txs.run();

    auto& info = txs.info;
    auto toLoc = [srcMgr = &info.srcMgr()](uint lsp_ln, uint lsp_col) -> clang::SourceLocation {
        auto fileID = srcMgr->getMainFileID();
        return srcMgr->translateLineCol(fileID, lsp_ln + 2, lsp_col + 2);
    };

    FoldingRangeParams param;
    auto res = feature::foldingRange(param, info);

    // dbg(res);

    txs.equal(res.size(), 2)
        //
        .expect("1", toLoc(res[0].startLine, res[0].startCharacter))
        .expect("2", toLoc(res[0].endLine, res[0].endCharacter))
        //
        .expect("3", toLoc(res[1].startLine, res[1].startCharacter))
        .expect("4", toLoc(res[1].endLine, res[1].endCharacter))
        //
        ;
}

TEST(FoldingRange, RecordDecl) {

    const char* main = R"cpp(
struct _2 {$(1)
    int x;
    float y;
};$(2)

struct _3 {};

struct _4;

union _5 {$(3)
    int x;
    float y;
};$(4)

)cpp";

    Tester txs("main.cpp", main);
    txs.run();

    auto& info = txs.info;
    auto toLoc = [srcMgr = &info.srcMgr()](uint lsp_ln, uint lsp_col) -> clang::SourceLocation {
        auto fileID = srcMgr->getMainFileID();
        return srcMgr->translateLineCol(fileID, lsp_ln + 2, lsp_col + 2);
    };

    FoldingRangeParams param;
    auto res = feature::foldingRange(param, info);

    // dbg(res);

    txs.equal(res.size(), 2)
        //
        .expect("1", toLoc(res[0].startLine, res[0].startCharacter))
        .expect("2", toLoc(res[0].endLine, res[0].endCharacter))
        //
        .expect("3", toLoc(res[1].startLine, res[1].startCharacter))
        .expect("4", toLoc(res[1].endLine, res[1].endCharacter))
        //
        ;
}

TEST(FoldingRange, CXXRecordDeclAndMemberMethod) {
    const char* main = R"cpp(
struct _2 {$(1)
    int x;
    float y;

    _2() = default;
};$(2)

struct _3 {$(3)
    void method() {$(5)
        int x = 0;
    }$(6)

    void parameter (){$(7)
        //
    }$(8)

};$(4)

struct _4;
)cpp";

    Tester txs("main.cpp", main);
    txs.run();

    auto& info = txs.info;
    auto toLoc = [srcMgr = &info.srcMgr()](uint lsp_ln, uint lsp_col) -> clang::SourceLocation {
        auto fileID = srcMgr->getMainFileID();
        return srcMgr->translateLineCol(fileID, lsp_ln + 2, lsp_col + 2);
    };

    FoldingRangeParams param;
    auto res = feature::foldingRange(param, info);

    // dbg(res);

    txs.equal(res.size(), 4)
        //
        .expect("1", toLoc(res[0].startLine, res[0].startCharacter))
        .expect("2", toLoc(res[0].endLine, res[0].endCharacter))
        //
        .expect("3", toLoc(res[1].startLine, res[1].startCharacter))
        .expect("4", toLoc(res[1].endLine, res[1].endCharacter))
        //
        .expect("5", toLoc(res[2].startLine, res[2].startCharacter))
        .expect("6", toLoc(res[2].endLine, res[2].endCharacter))
        //
        .expect("7", toLoc(res[3].startLine, res[3].startCharacter))
        .expect("8", toLoc(res[3].endLine, res[3].endCharacter))
        //
        // .expect("9", toLoc(res[4].startLine, res[4].startCharacter))
        // .expect("10", toLoc(res[4].endLine, res[4].endCharacter))
        //
        ;
}

TEST(FoldingRange, LambdaCapture) {
    const char* main = R"cpp(


)cpp";

    Tester txs("main.cpp", main);
    txs.run();

    auto& info = txs.info;
    auto toLoc = [srcMgr = &info.srcMgr()](uint lsp_ln, uint lsp_col) -> clang::SourceLocation {
        auto fileID = srcMgr->getMainFileID();
        return srcMgr->translateLineCol(fileID, lsp_ln + 2, lsp_col + 2);
    };

    FoldingRangeParams param;
    auto res = feature::foldingRange(param, info);

    dbg(res);

    // txs.equal(res.size(), 4)
    //     //
    //     .expect("1", toLoc(res[0].startLine, res[0].startCharacter))
    //     .expect("2", toLoc(res[0].endLine, res[0].endCharacter))
    //     //
    //     .expect("3", toLoc(res[1].startLine, res[1].startCharacter))
    //     .expect("4", toLoc(res[1].endLine, res[1].endCharacter))
    //     //
    //     .expect("5", toLoc(res[2].startLine, res[2].startCharacter))
    //     .expect("6", toLoc(res[2].endLine, res[2].endCharacter))
    //     //
    //     .expect("7", toLoc(res[3].startLine, res[3].startCharacter))
    //     .expect("8", toLoc(res[3].endLine, res[3].endCharacter))
    //     //
    //     .expect("9", toLoc(res[4].startLine, res[4].startCharacter))
    //     .expect("10", toLoc(res[4].endLine, res[4].endCharacter))
    //     //
    //     ;
}

TEST(FoldingRange, FnParas) {
    const char* main = R"cpp(


)cpp";

    Tester txs("main.cpp", main);
    txs.run();

    auto& info = txs.info;
    auto toLoc = [srcMgr = &info.srcMgr()](uint lsp_ln, uint lsp_col) -> clang::SourceLocation {
        auto fileID = srcMgr->getMainFileID();
        return srcMgr->translateLineCol(fileID, lsp_ln + 2, lsp_col + 2);
    };

    FoldingRangeParams param;
    auto res = feature::foldingRange(param, info);

    dbg(res);

    // txs.equal(res.size(), 4)
    //     //
    //     .expect("1", toLoc(res[0].startLine, res[0].startCharacter))
    //     .expect("2", toLoc(res[0].endLine, res[0].endCharacter))
    //     //
    //     .expect("3", toLoc(res[1].startLine, res[1].startCharacter))
    //     .expect("4", toLoc(res[1].endLine, res[1].endCharacter))
    //     //
    //     .expect("5", toLoc(res[2].startLine, res[2].startCharacter))
    //     .expect("6", toLoc(res[2].endLine, res[2].endCharacter))
    //     //
    //     .expect("7", toLoc(res[3].startLine, res[3].startCharacter))
    //     .expect("8", toLoc(res[3].endLine, res[3].endCharacter))
    //     //
    //     .expect("9", toLoc(res[4].startLine, res[4].startCharacter))
    //     .expect("10", toLoc(res[4].endLine, res[4].endCharacter))
    //     //
    //     ;
}

TEST(FoldingRange, FnBody) {
    const char* main = R"cpp(


)cpp";

    Tester txs("main.cpp", main);
    txs.run();

    auto& info = txs.info;
    auto toLoc = [srcMgr = &info.srcMgr()](uint lsp_ln, uint lsp_col) -> clang::SourceLocation {
        auto fileID = srcMgr->getMainFileID();
        return srcMgr->translateLineCol(fileID, lsp_ln + 2, lsp_col + 2);
    };

    FoldingRangeParams param;
    auto res = feature::foldingRange(param, info);

    dbg(res);

    // txs.equal(res.size(), 4)
    //     //
    //     .expect("1", toLoc(res[0].startLine, res[0].startCharacter))
    //     .expect("2", toLoc(res[0].endLine, res[0].endCharacter))
    //     //
    //     .expect("3", toLoc(res[1].startLine, res[1].startCharacter))
    //     .expect("4", toLoc(res[1].endLine, res[1].endCharacter))
    //     //
    //     .expect("5", toLoc(res[2].startLine, res[2].startCharacter))
    //     .expect("6", toLoc(res[2].endLine, res[2].endCharacter))
    //     //
    //     .expect("7", toLoc(res[3].startLine, res[3].startCharacter))
    //     .expect("8", toLoc(res[3].endLine, res[3].endCharacter))
    //     //
    //     .expect("9", toLoc(res[4].startLine, res[4].startCharacter))
    //     .expect("10", toLoc(res[4].endLine, res[4].endCharacter))
    //     //
    //     ;
}

TEST(FoldingRange, CompoundStmt) {
    const char* main = R"cpp(
#include <vector>

int main () {

    std::vector<int> _0 = {
        1, 2, 3,
    };


    std::vector<int> _1 = {
        1, 
        2, 
        3,
    };

}

)cpp";

    Tester txs("main.cpp", main);
    txs.run();

    auto& info = txs.info;
    auto toLoc = [srcMgr = &info.srcMgr()](uint lsp_ln, uint lsp_col) -> clang::SourceLocation {
        auto fileID = srcMgr->getMainFileID();
        return srcMgr->translateLineCol(fileID, lsp_ln + 2, lsp_col + 2);
    };

    FoldingRangeParams param;
    auto res = feature::foldingRange(param, info);

    dbg(res);

    // txs.equal(res.size(), 4)
    //     //
    //     .expect("1", toLoc(res[0].startLine, res[0].startCharacter))
    //     .expect("2", toLoc(res[0].endLine, res[0].endCharacter))
    //     //
    //     .expect("3", toLoc(res[1].startLine, res[1].startCharacter))
    //     .expect("4", toLoc(res[1].endLine, res[1].endCharacter))
    //     //
    //     .expect("5", toLoc(res[2].startLine, res[2].startCharacter))
    //     .expect("6", toLoc(res[2].endLine, res[2].endCharacter))
    //     //
    //     .expect("7", toLoc(res[3].startLine, res[3].startCharacter))
    //     .expect("8", toLoc(res[3].endLine, res[3].endCharacter))
    //     //
    //     .expect("9", toLoc(res[4].startLine, res[4].startCharacter))
    //     .expect("10", toLoc(res[4].endLine, res[4].endCharacter))
    //     //
    //     ;
}

TEST(FoldingRange, AccessControlBlock) {
    const char* main = R"cpp(
class _0 {
public:
    int x;

private:
    int z;
};

struct _1 {

    int x;

private:
    int z;
};

struct _2 {
public:
private:
public:
};
)cpp";

    Tester txs("main.cpp", main);
    txs.run();

    auto& info = txs.info;
    auto toLoc = [srcMgr = &info.srcMgr()](uint lsp_ln, uint lsp_col) -> clang::SourceLocation {
        auto fileID = srcMgr->getMainFileID();
        return srcMgr->translateLineCol(fileID, lsp_ln + 2, lsp_col + 2);
    };

    FoldingRangeParams param;
    auto res = feature::foldingRange(param, info);

    dbg(res);

    // txs.equal(res.size(), 4)
    //     //
    //     .expect("1", toLoc(res[0].startLine, res[0].startCharacter))
    //     .expect("2", toLoc(res[0].endLine, res[0].endCharacter))
    //     //
    //     .expect("3", toLoc(res[1].startLine, res[1].startCharacter))
    //     .expect("4", toLoc(res[1].endLine, res[1].endCharacter))
    //     //
    //     .expect("5", toLoc(res[2].startLine, res[2].startCharacter))
    //     .expect("6", toLoc(res[2].endLine, res[2].endCharacter))
    //     //
    //     .expect("7", toLoc(res[3].startLine, res[3].startCharacter))
    //     .expect("8", toLoc(res[3].endLine, res[3].endCharacter))
    //     //
    //     .expect("9", toLoc(res[4].startLine, res[4].startCharacter))
    //     .expect("10", toLoc(res[4].endLine, res[4].endCharacter))
    //     //
    //     ;
}

}  // namespace

}  // namespace clice
