#include "Test/CTest.h"
#include "Feature/FoldingRange.h"

namespace clice::testing {

namespace {

struct FoldingRange : public ::testing::Test {
    std::optional<Tester> tester;
    std::vector<feature::FoldingRange> result;

    void run(llvm::StringRef source) {
        tester.emplace("main.cpp", source);

        tester->run();
        auto& info = tester->info;

        result = feature::foldingRange(*info);
    }

    index::Shared<std::vector<feature::FoldingRange>> runWithHeader(llvm::StringRef source,
                                                                    llvm::StringRef header) {
        tester.emplace("main.cpp", source);
        tester->addFile(path::join(".", "header.h"), header);
        tester->run();
        auto& info = tester->info;
        return feature::indexFoldingRange(*info);
    }

    void EXPECT_RANGE(std::size_t index,
                      llvm::StringRef begin,
                      llvm::StringRef end,
                      feature::FoldingRangeKind kind,
                      std::source_location current = std::source_location::current()) {
        auto& folding = result[index];

        auto begOff = tester->offset(begin);
        EXPECT_EQ(begOff, folding.range.begin, current);

        auto endOff = tester->offset(end);
        EXPECT_EQ(endOff, folding.range.end, current);
    }
};

using enum feature::FoldingRangeKind::Kind;

TEST_F(FoldingRange, Namespace) {
    run(R"cpp(
namespace single_line $(1){ }$(2)

namespace with_nodes $(3){
    struct inner $(5){ }$(6);
}$(4)

namespace strange
                 $(7){

                 }$(8)

#define NS_BEGIN namespace ns {
#define NS_END }

$(9)NS_BEGIN
NS_END$(10)
)cpp");

    EXPECT_RANGE(0, "1", "2", Namespace);
    EXPECT_RANGE(1, "3", "4", Namespace);
    EXPECT_RANGE(2, "5", "6", Namespace);
    EXPECT_RANGE(3, "7", "8", Namespace);
    EXPECT_RANGE(4, "9", "10", Namespace);
}

TEST_F(FoldingRange, Enum) {
    run(R"cpp(
enum _0 $(1){
    A,
    B,
    C
}$(2);

enum class _1 $(3){
    A,
    B,
    C
}$(4);

enum _2 $(5){ D }$(6);

)cpp");

    EXPECT_RANGE(0, "1", "2", Enum);
    EXPECT_RANGE(1, "3", "4", Enum);
    EXPECT_RANGE(2, "5", "6", Enum);
}

TEST_F(FoldingRange, RecordDecl) {
    run(R"cpp(
struct s1 $(1){
    int x;
    float y;
}$(2);

struct s2 $(3){}$(4);

struct s3;

union u1 $(5){
    int x;
    float y;
}$(6);

struct u2 $(7){
    struct s4 $(9){

    }$(10);
}$(8);

void f() $(11){
    struct s5 $(13){

    }$(14);
}$(12)
)cpp");

    EXPECT_RANGE(0, "1", "2", Struct);
    EXPECT_RANGE(1, "3", "4", Struct);
    EXPECT_RANGE(2, "5", "6", Union);
    EXPECT_RANGE(3, "7", "8", Struct);
    EXPECT_RANGE(4, "9", "10", Struct);
    EXPECT_RANGE(5, "11", "12", FunctionBody);
    EXPECT_RANGE(6, "13", "14", Struct);
}

TEST_F(FoldingRange, CXXRecordDeclAndMemberMethod) {
    run(R"cpp(
struct _2 {$(1)
    int x;
    float y;

    _2() = default;
$(2)};

struct _3 {$(3)
    void method() {$(5)
        int x = 0;
    $(6)}

    void parameter () {$(7)
        //
    $(8)}

    void skip() {};
$(4)};

struct _4;
)cpp");

    EXPECT_RANGE(0, "1", "2", Struct);
    EXPECT_RANGE(1, "3", "4", Struct);
    EXPECT_RANGE(2, "5", "6", Struct);
    EXPECT_RANGE(3, "7", "8", Struct);
}

TEST_F(FoldingRange, LambdaCapture) {
    run(R"cpp(
auto z = [$(1)
    x = 0, y = 1
    $(2)]() {$(3)
    //
$(4)};

int array[4] = {0};

auto s = [$(5)
    x=0, 
    y = 1,
    z = array[
    0],
    k = -1
 $(6)](){ return; };

)cpp");

    EXPECT_EQ(result.size(), 3);

    EXPECT_RANGE(0, "1", "2", LambdaCapture);
    EXPECT_RANGE(1, "3", "4", LambdaCapture);
    EXPECT_RANGE(2, "5", "6", LambdaCapture);
}

TEST_F(FoldingRange, LambdaExpression) {
    run(R"cpp(
auto _0 = [](int _) {};

auto _1 = [](int _) {$(1)
    //
$(2)};

auto _2 = [](int _) {$(3)
    //
    return 0;
    $(4)};

auto _3 = []($(5)
        int _1,
        int _2
    $(6)) {};

)cpp");

    EXPECT_EQ(result.size(), 3);

    EXPECT_RANGE(0, "1", "2", FunctionBody);
    EXPECT_RANGE(1, "3", "4", FunctionBody);
    EXPECT_RANGE(2, "5", "6", FunctionBody);
}

TEST_F(FoldingRange, FunctionParams) {
    run(R"cpp(
void e() {}

void f($(1)
//
//
$(2)) {}

void g($(3)
int x,
int y = 2
//
$(4)) {}

void d($(5)
    int _1,
    int _2,
    ...
    $(6));
)cpp");

    EXPECT_RANGE(0, "1", "2", FunctionParams);
    EXPECT_RANGE(1, "3", "4", FunctionParams);
    EXPECT_RANGE(2, "5", "6", FunctionParams);
}

TEST_F(FoldingRange, FunctionBody) {
    run(R"cpp(
void f() {$(1)
//
//
$(2)}

void g() {$(3)
    int x = 0;
$(4)}

void e() {}

void n() {$(5)
    {$(7)
        // empty bock 
    $(8)}
    //
$(6)}
)cpp");
    EXPECT_EQ(result.size(), 4);

    EXPECT_RANGE(0, "1", "2", FunctionBody);
    EXPECT_RANGE(1, "3", "4", FunctionBody);
    EXPECT_RANGE(2, "5", "6", FunctionBody);
    EXPECT_RANGE(3, "7", "8", FunctionBody);
}

TEST_F(FoldingRange, FunctionCall) {
    run(R"cpp(
int f(int _1, int _2, int _3, int _4, int _5, int _6) { return _1 + _2; }

int main() {$(1)

    int _ = f(1, (1 + 2), 3, 4, 5, 6);

    return f($(3)
        1, 2, 3, 
        4, 5, 6
    $(4));
$(2)}
)cpp");

    EXPECT_RANGE(0, "1", "2", FunctionCall);
    EXPECT_RANGE(1, "3", "4", FunctionCall);
}

TEST_F(FoldingRange, CompoundStmt) {
    run(R"cpp(
int main () {$(1)

    {$(3)
        {$(5)
            //
        $(6)}

        {$(7)
            //
        $(8)}

        //
    $(4)}

    return 0;
$(2)}

)cpp");

    /// EXPECT_RANGE(0, "1", "2");
    /// EXPECT_RANGE(1, "3", "4");
    /// EXPECT_RANGE(2, "5", "6");
}

TEST_F(FoldingRange, InitializeList) {
    run(R"cpp(
struct L { int xs[4]; };

L l1 = {$(1)
    1, 2, 3, 4
$(2)};

L l2 = {$(3)
//
//
$(4)};

)cpp");

    EXPECT_RANGE(0, "1", "2", Initializer);
    EXPECT_RANGE(1, "3", "4", Initializer);
}

TEST_F(FoldingRange, AccessControlBlock) {
    run(R"cpp(
struct empty { int x; };

class _0 {$(1)
public:$(3)
    int x;

$(4)private:$(5)
    int z;
$(2)$(6)};

struct _1 {$(7)

    int x;

private:$(9)
    int z;
$(8)$(10)};

struct _2 {$(11)
public:
private:
public:$(13)

int x = 1;

$(12)$(14)};
)cpp");

    EXPECT_EQ(result.size(), 9);

    EXPECT_RANGE(0, "1", "2", AccessSpecifier);
    EXPECT_RANGE(1, "3", "4", AccessSpecifier);
    EXPECT_RANGE(2, "5", "6", AccessSpecifier);
    EXPECT_RANGE(3, "7", "8", AccessSpecifier);
    EXPECT_RANGE(4, "9", "10", AccessSpecifier);
    EXPECT_RANGE(5, "11", "12", AccessSpecifier);

    // do not test result[6] and result[7]

    EXPECT_RANGE(8, "13", "14", AccessSpecifier);
}

TEST_F(FoldingRange, Macro) {
    run(R"cpp(
#ifdef M1

#else

    #ifdef M2 
    
    
    #endif

#endif
)cpp");

    EXPECT_EQ(result.size(), 3);
}

TEST_F(FoldingRange, PragmaRegion) {
    run(R"cpp(
#pragma region level1 $(1)
    #pragma region level2 $(2)
        #pragma region level3 $(3)

        $(4)#pragma endregion level3

    
    $(5)#pragma endregion level2


$(6)#pragma endregion level1

#pragma endregion   // mismatch region, skipped

// broken region, use the end of file as endregion
#pragma region $(7)

$(eof))cpp");

    EXPECT_EQ(result.size(), 4);
    EXPECT_RANGE(0, "3", "4", Region);
    EXPECT_RANGE(1, "2", "5", Region);
    EXPECT_RANGE(2, "1", "6", Region);
    EXPECT_RANGE(3, "7", "eof", Region);
}

TEST_F(FoldingRange, WithHeader) {
    auto header = R"cpp(
namespace _1 {

namespace _2 {

}

}
)cpp";

    auto source = R"cpp(
#include "header.h"

int main() {$(3)

$(4)
}
)cpp";

    auto multifiles = runWithHeader(source, header);
    EXPECT_EQ(multifiles.size(), 2);

    auto mainID = tester->info->srcMgr().getMainFileID();
    for(auto& [id, result]: multifiles) {
        if(id == mainID) {
            EXPECT_EQ(result.size(), 1);
        } else {
            EXPECT_EQ(result.size(), 2);
        }
    }
}

}  // namespace

}  // namespace clice::testing
