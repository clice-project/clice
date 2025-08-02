#include "Test/Tester.h"
#include "Feature/FoldingRange.h"

namespace clice::testing {

namespace {

struct FoldingRange : TestFixture {
    std::vector<feature::FoldingRange> result;

    void run(llvm::StringRef source) {
        addMain("main.cpp", source);
        TestFixture::compile();
        result = feature::foldingRanges(*unit);
    }

    using Self = FoldingRange;

    void EXPECT_RANGE(this Self& self,
                      std::size_t index,
                      llvm::StringRef begin,
                      llvm::StringRef end,
                      feature::FoldingRangeKind kind,
                      LocationChain chain = LocationChain()) {
        auto& folding = self.result[index];
        auto begin_offset = self["main.cpp", begin];
        EXPECT_EQ(begin_offset, folding.range.begin, chain);
        auto end_offset = self["main.cpp", end];
        EXPECT_EQ(end_offset, folding.range.end, chain);
    }
};

using enum feature::FoldingRangeKind::Kind;

TEST_F(FoldingRange, Namespace) {
    run(R"cpp(
namespace single_line { }

namespace with_nodes $(1){
    struct inner $(3){ 
        int x;
    }$(4);
}$(2)

namespace strange
                 $(5){

                 }$(6)

#define NS_BEGIN namespace ns {
#define NS_END }

$(7)NS_BEGIN
NS_END$(8)
)cpp");

    EXPECT_EQ(result.size(), 4);
    EXPECT_RANGE(0, "1", "2", Namespace);
    EXPECT_RANGE(1, "3", "4", Namespace);
    EXPECT_RANGE(2, "5", "6", Namespace);
    EXPECT_RANGE(3, "7", "8", Namespace);
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

enum e3 { D };

)cpp");

    EXPECT_EQ(result.size(), 2);
    EXPECT_RANGE(0, "1", "2", Enum);
    EXPECT_RANGE(1, "3", "4", Enum);
}

TEST_F(FoldingRange, Record) {
    run(R"cpp(
struct s1 $(1){
    int x;
    float y;
}$(2);

struct s2 {};

struct s3;

union u1 $(3){
    int x;
    float y;
}$(4);

struct u2 $(5){
    struct s4 $(7){

    }$(8);
}$(6);

void foo() $(9){
    struct s5 $(11){

    }$(12);
}$(10)
)cpp");

    EXPECT_EQ(result.size(), 6);
    EXPECT_RANGE(0, "1", "2", Struct);
    EXPECT_RANGE(1, "3", "4", Union);
    EXPECT_RANGE(2, "5", "6", Struct);
    EXPECT_RANGE(3, "7", "8", Struct);
    EXPECT_RANGE(4, "9", "10", FunctionBody);
    EXPECT_RANGE(5, "11", "12", Struct);
}

TEST_F(FoldingRange, Method) {
    run(R"cpp(
struct s2 $(1){ 
    int x;
    float y;

    s2() = default;
}$(2);

struct s3;

struct s3 $(3){ 
    void method() $(5){ 
        int x = 0;
    }$(6)

    void parameter() $(7){ 
 
    }$(8)

    void skip() {};
}$(4);
)cpp");

    EXPECT_EQ(result.size(), 4);
    EXPECT_RANGE(0, "1", "2", Struct);
    EXPECT_RANGE(1, "3", "4", Struct);
    EXPECT_RANGE(2, "5", "6", FunctionBody);
    EXPECT_RANGE(3, "7", "8", FunctionBody);
}

TEST_F(FoldingRange, Lambda) {
    run(R"cpp(
auto z = $(1)[
    x = 0, y = 1
]$(2) () $(3){

}$(4);

static int array[4];

auto s = $(5)[
    x=0, 
    y = 1,
    z = array[
    0],
    k = -1
]$(6) () $(7){ 
    return; 
}$(8);

auto l1 = [] () {};

auto l2 = [] () $(9){
   
}$(10);

auto l3 = [] () $(11){
    return 0;
}$(12);

auto l4 = [] $(13)(
    int x1,
    int x2
)$(14) {};
)cpp");

    EXPECT_EQ(result.size(), 7);
    EXPECT_RANGE(0, "1", "2", LambdaCapture);
    EXPECT_RANGE(1, "3", "4", FunctionBody);
    EXPECT_RANGE(2, "5", "6", LambdaCapture);
    EXPECT_RANGE(3, "7", "8", FunctionBody);
    EXPECT_RANGE(4, "9", "10", FunctionBody);
    EXPECT_RANGE(5, "11", "12", FunctionBody);
    EXPECT_RANGE(6, "13", "14", FunctionBody);
}

TEST_F(FoldingRange, Function) {
    run(R"cpp(
void e() {};

void f $(1)(


)$(2) $(3){

}$(4)

void g $(5)(
    int x,
    int y = 2
)$(6) $(7){
    int z;
}$(8)

void h() $(9){
    int x = 0;
}$(10)

void i(  ) {   };

void j $(11)(
    int p1,
    int p2,
    ...
)$(12);

void k() $(13){

}$(14)
)cpp");

    EXPECT_EQ(result.size(), 7);
    EXPECT_RANGE(0, "1", "2", FunctionParams);
    EXPECT_RANGE(1, "3", "4", FunctionBody);
    EXPECT_RANGE(2, "5", "6", FunctionParams);
    EXPECT_RANGE(3, "7", "8", FunctionBody);
    EXPECT_RANGE(4, "9", "10", FunctionBody);
    EXPECT_RANGE(5, "11", "12", FunctionParams);
    EXPECT_RANGE(6, "13", "14", FunctionBody);
}

TEST_F(FoldingRange, FunctionCall) {
    run(R"cpp(
int f(int p1, int p2, int p3, int p4, int p5, int p6) { return p1 + p2; }

int main() $(1){
    int x = f(1, 2, 3, 4, 5, 6);
    
    int y = f $(2)(
        1, 2, 3,
        4, 5, 6
    )$(3);

    return f $(4)(
        1, 2, 3,
        4, 5, 6
    )$(5);
}$(6)
)cpp");

    EXPECT_EQ(result.size(), 3);
    EXPECT_RANGE(0, "1", "6", FunctionBody);
    EXPECT_RANGE(1, "2", "3", FunctionCall);
    EXPECT_RANGE(2, "4", "5", FunctionCall);
}

TEST_F(FoldingRange, CompoundStmt) {
    run(R"cpp(
int main() $(1){

    $(3){
        $(5){
            //
        }$(6)

        $(7){
            //
        }$(8)

        //
    }$(4)

    return 0;
}$(2)

)cpp");
}

TEST_F(FoldingRange, InitializeList) {
    run(R"cpp(
struct L { int xs[4]; };

L l1 = $(1){
    1, 2, 3, 4
}$(2);

L l2 = $(3){
//
//
}$(4);

)cpp");

    EXPECT_EQ(result.size(), 2);
    EXPECT_RANGE(0, "1", "2", Initializer);
    EXPECT_RANGE(1, "3", "4", Initializer);
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
    EXPECT_RANGE(0, "1", "6", Region);
    EXPECT_RANGE(1, "2", "5", Region);
    EXPECT_RANGE(2, "3", "4", Region);
}

}  // namespace

}  // namespace clice::testing
