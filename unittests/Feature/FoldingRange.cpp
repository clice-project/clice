#include "Test/CTest.h"
#include "Feature/FoldingRange.h"

namespace clice::testing {

namespace {

struct FoldingRange : public ::testing::Test {
    std::optional<Tester> tester;
    proto::FoldingRangeResult result;

    void run(llvm::StringRef code) {
        tester.emplace("main.cpp", code);
        tester->run();
        auto& info = tester->info;

        FoldingRangeParams param;
        SourceConverter converter;
        result = feature::foldingRange(param, info, converter);
    }

    void EXPECT_RANGE(std::size_t index,
                      llvm::StringRef begin,
                      llvm::StringRef end,
                      std::source_location current = std::source_location::current()) {
        auto& folding = result[index];
        EXPECT_EQ(tester->pos(begin),
                  proto::Position{folding.startLine, folding.startCharacter},
                  current);
        EXPECT_EQ(tester->pos(end),
                  proto::Position{folding.endLine, folding.endCharacter},
                  current);
    }
};

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

TEST_F(FoldingRange, Namespace) {
    run(R"cpp(
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

)cpp");

    EXPECT_RANGE(0, "1", "2");
    EXPECT_RANGE(1, "3", "4");
    EXPECT_RANGE(2, "5", "6");
}

TEST_F(FoldingRange, Enum) {
    run(R"cpp(
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

)cpp");

    EXPECT_RANGE(0, "1", "2");
    EXPECT_RANGE(1, "3", "4");
}

TEST_F(FoldingRange, RecordDecl) {
    run(R"cpp(
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

)cpp");

    EXPECT_RANGE(0, "1", "2");
    EXPECT_RANGE(1, "3", "4");
    EXPECT_RANGE(2, "5", "6");
    EXPECT_RANGE(3, "7", "8");
    EXPECT_RANGE(4, "9", "10");
    EXPECT_RANGE(5, "11", "12");
}

TEST_F(FoldingRange, CXXRecordDeclAndMemberMethod) {
    run(R"cpp(
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
)cpp");

    EXPECT_RANGE(0, "1", "2");
    EXPECT_RANGE(1, "3", "4");
    EXPECT_RANGE(2, "5", "6");
    EXPECT_RANGE(3, "7", "8");
}

TEST_F(FoldingRange, LambdaCapture) {
    run(R"cpp(
auto z = [$(1)
    x = 0, y = 1$(2)
]() {$(3)
    //$(4)
};

auto s = [$(5)
    x=0, 
    y = 1$(6)
](){ return; };

)cpp");

    EXPECT_EQ(result.size(), 3);

    EXPECT_RANGE(0, "1", "2");
    EXPECT_RANGE(1, "3", "4");
    EXPECT_RANGE(2, "5", "6");
}

TEST_F(FoldingRange, LambdaExpression) {
    run(R"cpp(
auto _0 = [](int _) {};

auto _1 = [](int _) {$(1)
    //$(2)
};

auto _2 = [](int _) {$(3)
    //
    return 0;$(4)
};

auto _3 = []($(5)
        int _1,
        int _2$(6)
    ) {};

)cpp");

    EXPECT_RANGE(0, "1", "2");
    EXPECT_RANGE(1, "3", "4");
    EXPECT_RANGE(2, "5", "6");
}

TEST_F(FoldingRange, FunctionParams) {
    run(R"cpp(
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

void d($(5)
    int _1,
    int _2,
    ...$(6)
);
)cpp");

    EXPECT_RANGE(0, "1", "2");
    EXPECT_RANGE(1, "3", "4");
    EXPECT_RANGE(2, "5", "6");
}

TEST_F(FoldingRange, FunctionBody) {
    run(R"cpp(
void f() {$(1)
//
//$(2)
}

void g() {$(3)
    int x = 0;$(4)
}

void e() {}

void n() {$(5)
    {$(7)
        // empty bock $(8)
    }
    //$(6)
}
)cpp");

    EXPECT_RANGE(0, "1", "2");
    EXPECT_RANGE(1, "3", "4");
    EXPECT_RANGE(2, "5", "6");
    EXPECT_RANGE(3, "7", "8");
}

TEST_F(FoldingRange, FunctionCall) {
    run(R"cpp(
int f(int _1, int _2, int _3, int _4, int _5, int _6) { return _1 + _2; }

int main() {$(1)

    int _ = f(1, (1 + 2), 3, 4, 5, 6);

    return f($(3)
        1, 2, 3, 
        4, 5, 6$(4)
    );$(2)
}
)cpp");

    EXPECT_RANGE(0, "1", "2");
    EXPECT_RANGE(1, "3", "4");
}

TEST_F(FoldingRange, CompoundStmt) {
    run(R"cpp(
int main () {$(1)

    {$(3)
        {$(5)
            //$(6)
        }

        {$(7)
            //$(8)
        }

        //$(4)
    }

    return 0;$(2)
}

)cpp");

    EXPECT_RANGE(0, "1", "2");
    EXPECT_RANGE(1, "3", "4");
    EXPECT_RANGE(2, "5", "6");
}

TEST_F(FoldingRange, InitializeList) {
    run(R"cpp(
struct L { int xs[4]; };

L l1 = {$(1)
    1, 2, 3, 4$(2)
};

L l2 = {$(3)
//
//$(4)
};

)cpp");

    EXPECT_RANGE(0, "1", "2");
    EXPECT_RANGE(1, "3", "4");
}

TEST_F(FoldingRange, AccessControlBlock) {
    run(R"cpp(
struct empty { int x; };

class _0 {$(1)
public:$(3)
    int x;$(4)
private:$(5)
    int z;$(2)$(6)
};

struct _1 {$(7)

    int x;

private:$(9)
    int z;$(8)$(10)
};

struct _2 {$(11)
public:
private:
public:$(12)
};
)cpp");

    EXPECT_RANGE(0, "1", "2");
    EXPECT_RANGE(1, "3", "4");
    EXPECT_RANGE(2, "5", "6");
    EXPECT_RANGE(3, "7", "8");
    EXPECT_RANGE(4, "9", "10");
    EXPECT_RANGE(5, "11", "12");
}

TEST_F(FoldingRange, Macro) {
    run(R"cpp(
#$(1)ifdef M1
$(2)
#$(3)else

    #$(5)ifdef M2
    
    //$(6)
    #endif

//$(4)
#endif
)cpp");

    EXPECT_RANGE(0, "1", "2");
    EXPECT_RANGE(1, "5", "6");
    EXPECT_RANGE(2, "3", "4");
}

}  // namespace

}  // namespace clice::testing
