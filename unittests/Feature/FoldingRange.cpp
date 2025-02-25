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
enum e1 $(1){
    A,
    B,
    C
}$(2);

enum class e2 $(3){
    A,
    B,
    C
}$(4);

enum e3 $(5){ D }$(6);

)cpp");

    EXPECT_RANGE(0, "1", "2", Enum);
    EXPECT_RANGE(1, "3", "4", Enum);
    EXPECT_RANGE(2, "5", "6", Enum);
}

TEST_F(FoldingRange, Record) {
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

void foo$(11)()$(12) $(13){
    struct s5 $(15){

    }$(16);
}$(14)
)cpp");

    EXPECT_RANGE(0, "1", "2", Struct);
    EXPECT_RANGE(1, "3", "4", Struct);
    EXPECT_RANGE(2, "5", "6", Union);
    EXPECT_RANGE(3, "7", "8", Struct);
    EXPECT_RANGE(4, "9", "10", Struct);
    EXPECT_RANGE(5, "11", "12", FunctionParams);
    EXPECT_RANGE(6, "13", "14", FunctionBody);
    EXPECT_RANGE(7, "15", "16", Struct);
}

TEST_F(FoldingRange, Method) {
    run(R"cpp(
struct s2 $(1){ 
    int x;
    float y;

    s2$(3)()$(4) = default;
}$(2);

struct s3;

struct s3 $(5){ 
    void method$(7)()$(8) $(9){ 
        int x = 0;
    }$(10)

    void parameter$(11)()$(12) $(13){ 
 
    }$(14)

    void skip$(15)()$(16) {};
}$(6);
)cpp");

    EXPECT_RANGE(0, "1", "2", Struct);
    EXPECT_RANGE(1, "3", "4", FunctionParams);
    EXPECT_RANGE(2, "5", "6", Struct);
    EXPECT_RANGE(3, "7", "8", FunctionParams);
    EXPECT_RANGE(4, "9", "10", FunctionBody);
    EXPECT_RANGE(5, "11", "12", FunctionParams);
    EXPECT_RANGE(6, "13", "14", FunctionBody);
    EXPECT_RANGE(7, "15", "16", FunctionParams);
}

TEST_F(FoldingRange, Lambda) {
    run(R"cpp(
auto z = $(1)[
    x = 0, y = 1
]$(2) $(3)()$(4) $(5){

}$(6);

static int array[4];

auto s = $(7)[
    x=0, 
    y = 1,
    z = array[
    0],
    k = -1
]$(8) $(9)()$(10) $(11){ return; }$(12);

auto l1 = $(13)[]$(14) $(15)()$(16) $(17){}$(18);

auto l2 = $(19)[]$(20) $(21)()$(22) $(23){
    //
}$(24);

auto l3 = $(25)[]$(26) $(27)()$(28) $(29){
    //
    return 0;
}$(30);

auto l4 = $(31)[]$(32) $(33)(
        int x1,
        int x2
)$(34) $(35){}$(36);

)cpp");

    EXPECT_RANGE(0, "1", "2", LambdaCapture);
    EXPECT_RANGE(1, "3", "4", FunctionParams);
    EXPECT_RANGE(2, "5", "6", FunctionBody);
    EXPECT_RANGE(3, "7", "8", LambdaCapture);
    EXPECT_RANGE(4, "9", "10", FunctionParams);
    EXPECT_RANGE(5, "11", "12", FunctionBody);
    EXPECT_RANGE(6, "13", "14", LambdaCapture);
    EXPECT_RANGE(7, "15", "16", FunctionParams);
    EXPECT_RANGE(8, "17", "18", FunctionBody);
    EXPECT_RANGE(9, "19", "20", LambdaCapture);
    EXPECT_RANGE(10, "21", "22", FunctionParams);
    EXPECT_RANGE(11, "23", "24", FunctionBody);
    EXPECT_RANGE(12, "25", "26", LambdaCapture);
    EXPECT_RANGE(13, "27", "28", FunctionParams);
    EXPECT_RANGE(14, "29", "30", FunctionBody);
    EXPECT_RANGE(15, "31", "32", LambdaCapture);
    EXPECT_RANGE(16, "33", "34", FunctionParams);
    EXPECT_RANGE(17, "35", "36", FunctionBody);
}

TEST_F(FoldingRange, FunctionParams) {
    run(R"cpp(
void e $(1)()$(2) $(3){}$(4)

void f $(5)(


)$(6) $(7){}$(8)

void g $(9)(
int x,
int y = 2
//
)$(10) $(11){}$(12)

void d $(13)(
    int p1,
    int p2,
    ...
)$(14);
)cpp");

    EXPECT_RANGE(0, "1", "2", FunctionParams);
    EXPECT_RANGE(1, "3", "4", FunctionBody);
    EXPECT_RANGE(2, "5", "6", FunctionParams);
    EXPECT_RANGE(3, "7", "8", FunctionBody);
    EXPECT_RANGE(4, "9", "10", FunctionParams);
    EXPECT_RANGE(5, "11", "12", FunctionBody);
    EXPECT_RANGE(6, "13", "14", FunctionParams);
}

TEST_F(FoldingRange, FunctionBody) {
    run(R"cpp(
void f $(1)()$(2) $(3){

}$(4)

void g $(5)()$(6) $(7){
    int x = 0;
}$(8)

void e $(9)()$(10) $(11){}$(12)

void n $(13)()$(14) $(15){
    $(17){
        
    }$(18)
}$(16)
)cpp");
    EXPECT_RANGE(0, "1", "2", FunctionParams);
    EXPECT_RANGE(1, "3", "4", FunctionBody);
    EXPECT_RANGE(2, "5", "6", FunctionParams);
    EXPECT_RANGE(3, "7", "8", FunctionBody);
    EXPECT_RANGE(4, "9", "10", FunctionParams);
    EXPECT_RANGE(5, "11", "12", FunctionBody);
    EXPECT_RANGE(6, "13", "14", FunctionParams);
    EXPECT_RANGE(7, "15", "16", FunctionBody);
    /// FIXME: Add compound stmt EXPECT_RANGE(8, "17", "18", FunctionBody);
}

TEST_F(FoldingRange, FunctionCall) {
    run(R"cpp(
int f $(1)(int p1, int p2, int p3, int p4, int p5, int p6)$(2) $(3){ return p1 + p2; }$(4)

int main $(5)()$(6) $(7){

    int _ = f $(9)(1, (1 + 2), 3, 4, 5, 6)$(10);

    return f $(11)(
        1, 2, 3, 
        4, 5, 6
    )$(12);
}$(8)
)cpp");

    EXPECT_RANGE(0, "1", "2", FunctionParams);
    EXPECT_RANGE(1, "3", "4", FunctionBody);
    EXPECT_RANGE(2, "5", "6", FunctionParams);
    EXPECT_RANGE(3, "7", "8", FunctionBody);
    EXPECT_RANGE(4, "9", "10", FunctionCall);
    EXPECT_RANGE(5, "11", "12", FunctionCall);
}

TEST_F(FoldingRange, CompoundStmt) {
    run(R"cpp(
int main $(1)()$(2) $(3){

    $(5){
        $(7){
            //
        }$(8)

        $(9){
            //
        }$(10)

        //
    }$(6)

    return 0;
}$(4)

)cpp");

    /// EXPECT_RANGE(0, "1", "2", FunctionParams);
    /// EXPECT_RANGE(1, "3", "4", FunctionBody);
    /// EXPECT_RANGE(2, "5", "6", FunctionBody);
    /// EXPECT_RANGE(3, "7", "8", FunctionBody);
    /// EXPECT_RANGE(4, "9", "10", FunctionBody);
}

TEST_F(FoldingRange, InitializeList) {
    run(R"cpp(
struct L $(1){ int xs[4]; }$(2);

L l1 = $(3){
    1, 2, 3, 4
}$(4);

L l2 = $(5){
//
//
}$(6);

)cpp");

    EXPECT_RANGE(0, "1", "2", Struct);
    EXPECT_RANGE(1, "3", "4", Initializer);
    EXPECT_RANGE(2, "5", "6", Initializer);
}

TEST_F(FoldingRange, AccessSpecifier) {
    run(R"cpp(
class c1 $(1){
public$(3):
private$(4):
protected$(5):
}$(2);

class c2 $(6){
public$(8):
    int x;

private$(9):
    float y;

protected$(10):
    double z;
}$(7);

#define PUBLIC public:
#define PRIVATE private:
#define PROTECTED protected:

class c3 $(11){
$(13)PUBLIC
    int a;

$(15)PRIVATE$(14)
    int b;

$(17)PROTECTED$(16)
    int c;
}$(12);
)cpp");

    EXPECT_RANGE(0, "1", "2", Class);
    EXPECT_RANGE(1, "3", "4", AccessSpecifier);
    EXPECT_RANGE(2, "4", "5", AccessSpecifier);
    EXPECT_RANGE(3, "5", "2", AccessSpecifier);

    EXPECT_RANGE(4, "6", "7", Class);
    EXPECT_RANGE(5, "8", "9", AccessSpecifier);
    EXPECT_RANGE(6, "9", "10", AccessSpecifier);
    EXPECT_RANGE(7, "10", "7", AccessSpecifier);

    EXPECT_RANGE(8, "11", "12", Class);
    EXPECT_RANGE(9, "13", "14", AccessSpecifier);
    EXPECT_RANGE(10, "15", "16", AccessSpecifier);
    EXPECT_RANGE(11, "17", "12", AccessSpecifier);
}

TEST_F(FoldingRange, Directive) {
    run(R"cpp(
#ifdef M1

#else

    #ifdef M2 
    
    
    #endif

#endif
)cpp");

    /// EXPECT_EQ(result.size(), 3);
    /// FIXME: Add directive folding range.
}

TEST_F(FoldingRange, PragmaRegion) {
    run(R"cpp(
$(1)#pragma region level1
    $(2)#pragma region level2 
        $(3)#pragma region level3 

        #$(4)pragma endregion level3

    
    #$(5)pragma endregion level2


#$(6)pragma endregion level1

#pragma endregion   // mismatch region, skipped
#pragma region  // mismatch region, skipped
)cpp");

    EXPECT_EQ(result.size(), 3);
    /// FIXME: Modify Pragma range.
    EXPECT_RANGE(0, "3", "4", Region);
    EXPECT_RANGE(1, "2", "5", Region);
    EXPECT_RANGE(2, "1", "6", Region);
}

}  // namespace

}  // namespace clice::testing
