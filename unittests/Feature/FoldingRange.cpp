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

// convert 0-0 based location in LSP to 1-1 based location in clang.
auto fromLspLocation(const clang::SourceManager* src, proto::FoldingRange range)
    -> std::pair<clang::SourceLocation, clang::SourceLocation> {
    auto fileID = src->getMainFileID();
    return {src->translateLineCol(fileID, range.startLine + 1, range.startCharacter + 1),
            src->translateLineCol(fileID, range.endLine + 1, range.endCharacter + 1)};
}

TEST(FoldingRange, Namespace) {
    const char* main = R"cpp(
namespace single_line {$(1)
//$(2)
}

namespace with_nodes {$(3)
//
//struct _ {};$(4)
}

namespace empty {}

namespace ugly 

{$(5)
//
//$(6)
}

)cpp";

    Tester txs("main.cpp", main);
    txs.run();

    auto& info = txs.info;

    FoldingRangeParams param;
    auto res = feature::foldingRange(param, info);

    // dbg(res);

    auto toLoc = [src = &info.srcMgr()](const proto::FoldingRange& fr) {
        return fromLspLocation(src, fr);
    };

    txs.equal(res.size(), 3)
        //
        .expect("1", toLoc(res[0]).first)
        .expect("2", toLoc(res[0]).second)
        //
        .expect("3", toLoc(res[1]).first)
        .expect("4", toLoc(res[1]).second)
        //
        .expect("5", toLoc(res[2]).first)
        .expect("6", toLoc(res[2]).second)
        //
        ;
}

TEST(FoldingRange, Enum) {
    auto main = R"cpp(
enum _0 {$(1)
    A,
    B,
    C$(2)
};

enum _1 { D };

enum class _2 {$(3)
    A,
    B,
    C$(4)
};

)cpp";

    Tester txs("main.cpp", main);
    txs.run();

    auto& info = txs.info;
    auto toLoc = [src = &info.srcMgr()](const proto::FoldingRange& fr) {
        return fromLspLocation(src, fr);
    };

    FoldingRangeParams param;
    auto res = feature::foldingRange(param, info);

    // dbg(res);

    txs.equal(res.size(), 2)
        //
        .expect("1", toLoc(res[0]).first)
        .expect("2", toLoc(res[0]).second)
        //
        .expect("3", toLoc(res[1]).first)
        .expect("4", toLoc(res[1]).second)
        //
        ;
}

TEST(FoldingRange, RecordDecl) {
    const char* main = R"cpp(
struct _2 {$(1)
    int x;
    float y;$(2)
};

struct _3 {};

struct _4;

union _5 {$(3)
    int x;
    float y;$(4)
};

struct _6 {$(5)
    struct nested {$(7)
        //$(8)
    };
    
    //$(6)
};

void f() {$(9)
    struct nested {$(11)
        //$(12)
    };$(10)
}

)cpp";

    Tester txs("main.cpp", main);
    txs.run();

    auto& info = txs.info;
    auto toLoc = [src = &info.srcMgr()](const proto::FoldingRange& fr) {
        return fromLspLocation(src, fr);
    };

    FoldingRangeParams param;
    auto res = feature::foldingRange(param, info);

    // dbg(res);

    txs.equal(res.size(), 6)
        //
        .expect("1", toLoc(res[0]).first)
        .expect("2", toLoc(res[0]).second)
        //
        .expect("3", toLoc(res[1]).first)
        .expect("4", toLoc(res[1]).second)
        //
        .expect("5", toLoc(res[2]).first)
        .expect("6", toLoc(res[2]).second)
        //
        .expect("7", toLoc(res[3]).first)
        .expect("8", toLoc(res[3]).second)
        //
        .expect("9", toLoc(res[4]).first)
        .expect("10", toLoc(res[4]).second)
        //
        .expect("11", toLoc(res[5]).first)
        .expect("12", toLoc(res[5]).second)
        //
        ;
}

TEST(FoldingRange, CXXRecordDeclAndMemberMethod) {
    const char* main = R"cpp(
struct _2 {$(1)
    int x;
    float y;

    _2() = default;$(2)
};

struct _3 {$(3)
    void method() {$(5)
        int x = 0;$(6)
    }

    void parameter (){$(7)
        //$(8)
    }

    void skip() {};
$(4)
};

struct _4;
)cpp";

    Tester txs("main.cpp", main);
    txs.run();

    auto& info = txs.info;
    auto toLoc = [src = &info.srcMgr()](const proto::FoldingRange& fr) {
        return fromLspLocation(src, fr);
    };

    FoldingRangeParams param;
    auto res = feature::foldingRange(param, info);

    // dbg(res);

    txs.equal(res.size(), 4)
        //
        .expect("1", toLoc(res[0]).first)
        .expect("2", toLoc(res[0]).second)
        //
        .expect("3", toLoc(res[1]).first)
        .expect("4", toLoc(res[1]).second)
        //
        .expect("5", toLoc(res[2]).first)
        .expect("6", toLoc(res[2]).second)
        //
        .expect("7", toLoc(res[3]).first)
        .expect("8", toLoc(res[3]).second)
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
    auto toLoc = [src = &info.srcMgr()](const proto::FoldingRange& fr) {
        return fromLspLocation(src, fr);
    };

    FoldingRangeParams param;
    auto res = feature::foldingRange(param, info);

    dbg(res);

    // txs.equal(res.size(), 4)
    //     //
    //     .expect("1", toLoc(res[0]).first)
    //     .expect("2", toLoc(res[0]).first)
    //     //
    //     .expect("3", toLoc(res[1]).first)
    //     .expect("4", toLoc(res[1]).second)
    //     //
    //     .expect("5", toLoc(res[2]).first)
    //     .expect("6", toLoc(res[2]).second)
    //     //
    //     .expect("7", toLoc(res[3]).first)
    //     .expect("8", toLoc(res[3]).second)
    //     //
    //     .expect("9", toLoc(res[4].startLine, res[4].startCharacter))
    //     .expect("10", toLoc(res[4].endLine, res[4].endCharacter))
    //     //
    //     ;
}

TEST(FoldingRange, FnParas) {
    const char* main = R"cpp(
void e() {}

void f($(1)
//
//$(2)
) {}

void g($(3)
int x,
int y = 2
//$(4)
) {}
)cpp";

    Tester txs("main.cpp", main);
    txs.run();

    auto& info = txs.info;
    auto toLoc = [src = &info.srcMgr()](const proto::FoldingRange& fr) {
        return fromLspLocation(src, fr);
    };

    FoldingRangeParams param;
    auto res = feature::foldingRange(param, info);

    dbg(res);

    // txs.equal(res.size(), 2)
    //     //
    //     .expect("1", toLoc(res[0]).first)
    //     .expect("2", toLoc(res[0]).first)
    //     //
    //     .expect("3", toLoc(res[1]).first)
    //     .expect("4", toLoc(res[1]).second)
    //     //
    //     ;
}

TEST(FoldingRange, FnBody) {
    const char* main = R"cpp(

void f() {$(1)
//
//$(2)
}

void g() {$(3)
    int x = 0;$(4)
}

void e() {}

void n() {$(5)
    { // inner block 

    }$(6)
}
)cpp";

    Tester txs("main.cpp", main);
    txs.run();

    auto& info = txs.info;
    auto toLoc = [src = &info.srcMgr()](const proto::FoldingRange& fr) {
        return fromLspLocation(src, fr);
    };

    FoldingRangeParams param;
    auto res = feature::foldingRange(param, info);

    // dbg(res);

    txs.equal(res.size(), 3)
        //
        .expect("1", toLoc(res[0]).first)
        .expect("2", toLoc(res[0]).second)
        //
        .expect("3", toLoc(res[1]).first)
        .expect("4", toLoc(res[1]).second)
        //
        .expect("5", toLoc(res[2]).first)
        .expect("6", toLoc(res[2]).second)
        //
        ;
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
    auto toLoc = [src = &info.srcMgr()](const proto::FoldingRange& fr) {
        return fromLspLocation(src, fr);
    };

    FoldingRangeParams param;
    auto res = feature::foldingRange(param, info);

    dbg(res);

    // txs.equal(res.size(), 4)
    //     //
    //     .expect("1", toLoc(res[0]).first)
    //     .expect("2", toLoc(res[0]).first)
    //     //
    //     .expect("3", toLoc(res[1]).first)
    //     .expect("4", toLoc(res[1]).second)
    //     //
    //     .expect("5", toLoc(res[2]).first)
    //     .expect("6", toLoc(res[2]).second)
    //     //
    //     .expect("7", toLoc(res[3]).first)
    //     .expect("8", toLoc(res[3]).second)
    //     //
    //     .expect("9", toLoc(res[4].startLine, res[4].startCharacter))
    //     .expect("10", toLoc(res[4].endLine, res[4].endCharacter))
    //     //
    //     ;
}

TEST(FoldingRange, AccessControlBlock) {
    const char* main = R"cpp(
// struct empty { int x; };

class _0 {$(1)
public:$(3)
    int x;$(4)
private:$(5)
    int z;$(2)$(6)
};

// struct _1 {

//     int x;

// private:
//     int z;
// };

// struct _2 {
// public:
// private:
// public:
// };
)cpp";

    Tester txs("main.cpp", main);
    txs.run();

    auto& info = txs.info;
    auto toLoc = [src = &info.srcMgr()](const proto::FoldingRange& fr) {
        return fromLspLocation(src, fr);
    };

    // FoldingRangeParams param;
    // auto res = feature::foldingRange(param, info);

    // dbg(res);

    // txs.equal(res.size(), 3)
    //     //
    //     .expect("1", toLoc(res[0]).first)
    //     .expect("2", toLoc(res[0]).second)
    //     //
    //     .expect("3", toLoc(res[1]).first)
    //     .expect("4", toLoc(res[1]).second)
    //     //
    //     .expect("5", toLoc(res[2]).first)
    //     .expect("6", toLoc(res[2]).second)
    //
    // .expect("7", toLoc(res[3]).first)
    // .expect("8", toLoc(res[3]).second)
    //
    // .expect("9", toLoc(res[4].startLine, res[4].startCharacter))
    // .expect("10", toLoc(res[4].endLine, res[4].endCharacter))
    //
    ;
}

}  // namespace

}  // namespace clice
