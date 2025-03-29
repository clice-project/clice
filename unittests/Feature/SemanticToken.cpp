#include "Test/CTest.h"
#include "Feature/SemanticToken.h"

namespace clice::testing {

namespace {

struct SemanticToken : TestFixture {
    index::Shared<feature::SemanticTokens> result;

    void run(llvm::StringRef code) {
        addMain("main.cpp", code);
        Tester::compile();
        result = feature::indexSemanticTokens(*AST);
    }

    void EXPECT_TOKEN(llvm::StringRef pos,
                      SymbolKind kind,
                      uint32_t length,
                      LocationChain chain = LocationChain()) {
        bool visited = false;
        auto offset = offsets[pos];
        auto& tokens = result[AST->getInterestedFile()];

        for(auto& token: tokens) {
            if(token.range.begin == offset) {
                EXPECT_EQ(token.kind, kind, chain);
                EXPECT_EQ(token.range.end - token.range.begin, length, chain);
                visited = true;
                break;
            }
        }

        EXPECT_EQ(visited, true, chain);
    }

    void EXPECT_TOKEN(llvm::StringRef pos,
                      SymbolKind kind,
                      SymbolModifiers modifiers,
                      uint32_t length,
                      LocationChain chain = LocationChain()) {
        bool visited = false;
        auto offset = offsets[pos];
        auto& tokens = result[AST->getInterestedFile()];

        for(auto& token: tokens) {
            if(token.range.begin == offset) {
                EXPECT_EQ(token.kind, kind, chain);
                EXPECT_EQ(token.range.end - token.range.begin, length, chain);
                EXPECT_EQ(token.modifiers, modifiers, chain);
                visited = true;
                break;
            }
        }

        EXPECT_EQ(visited, true, chain);
    }

    void dumpResult() {
        auto& tokens = result[AST->getInterestedFile()];
        for(auto& token: tokens) {
            clice::println("token: {}", dump(token));
        }
    }
};

using enum SymbolKind::Kind;
using enum SymbolModifiers::Kind;

TEST_F(SemanticToken, Include) {
    run(R"cpp(
$(0)#include $(1)<stddef.h>
$(2)#include $(3)"stddef.h"
$(4)# $(5)include $(6)"stddef.h"
)cpp");

    /// FIXME: Included file could be macro.

    EXPECT_TOKEN("0", Directive, 8);
    EXPECT_TOKEN("1", Header, 10);
    EXPECT_TOKEN("2", Directive, 8);
    EXPECT_TOKEN("3", Header, 10);
    EXPECT_TOKEN("4", Directive, 1);
    EXPECT_TOKEN("5", Directive, 7);
    EXPECT_TOKEN("6", Header, 10);
}

TEST_F(SemanticToken, Comment) {
    run(R"cpp(
$(line)/// line comment
int x = 1;
)cpp");

    EXPECT_TOKEN("line", Comment, 16);
}

TEST_F(SemanticToken, Keyword) {
    run(R"cpp(
$(int)int main() {
    $(return)return 0;
}
)cpp");

    EXPECT_TOKEN("int", Keyword, 3);
    EXPECT_TOKEN("return", Keyword, 6);
}

TEST_F(SemanticToken, Macro) {
    run(R"cpp(
$(0)#define $(macro)FOO
)cpp");

    EXPECT_TOKEN("0", Directive, 7);
    EXPECT_TOKEN("macro", Macro, 3);
}

TEST_F(SemanticToken, FinalAndOverride) {
    run(R"cpp(
struct A $(0)final {};

struct B {
    virtual void foo();
};

struct C : B {
    void foo() $(1)override;
};

struct D : C {
    void foo() $(2)final;
};
)cpp");

    EXPECT_TOKEN("0", Keyword, 5);
    EXPECT_TOKEN("1", Keyword, 8);
    EXPECT_TOKEN("2", Keyword, 5);
}

TEST_F(SemanticToken, VarDecl) {
    run(R"cpp(
extern int $(0)x;

int $(1)x = 1;

template <typename T, typename U>
extern int $(2)y;

template <typename T, typename U>
int $(3)y = 2;

template<typename T>
extern int $(4)y<T, int>;

template<typename T>
int $(5)y<T, int> = 4;

template<>
int $(6)y<int, int> = 5;

int main() {
    $(7)x = 6;
}
)cpp");

    EXPECT_TOKEN("0", Variable, Declaration, 1);
    EXPECT_TOKEN("1", Variable, Definition, 1);
    EXPECT_TOKEN("2", Variable, SymbolModifiers(Declaration, Templated), 1);
    EXPECT_TOKEN("3", Variable, SymbolModifiers(Definition, Templated), 1);
    EXPECT_TOKEN("4", Variable, SymbolModifiers(Declaration, Templated), 1);
    EXPECT_TOKEN("5", Variable, SymbolModifiers(Definition, Templated), 1);
    EXPECT_TOKEN("6", Variable, Definition, 1);
    EXPECT_TOKEN("7", Variable, SymbolModifiers(), 1);
}

TEST_F(SemanticToken, FunctionDecl) {
    run(R"cpp(
extern int $(0)foo();

int $(1)foo() {
    return 0;
}

template <typename T>
extern int $(2)bar();

template <typename T>
int $(3)bar() {
    return 1;
}
)cpp");

    EXPECT_TOKEN("0", Function, Declaration, 3);
    EXPECT_TOKEN("1", Function, Definition, 3);
    EXPECT_TOKEN("2", Function, SymbolModifiers(Declaration, Templated), 3);
    EXPECT_TOKEN("3", Function, SymbolModifiers(Definition, Templated), 3);
}

TEST_F(SemanticToken, RecordDecl) {
    run(R"cpp(
class $(0)A;

class $(1)A {};

struct $(2)B;

struct $(3)B {};

union $(4)C;

union $(5)C {};
)cpp");

    EXPECT_TOKEN("0", Class, Declaration, 1);
    EXPECT_TOKEN("1", Class, Definition, 1);
    EXPECT_TOKEN("2", Struct, Declaration, 1);
    EXPECT_TOKEN("3", Struct, Definition, 1);
    EXPECT_TOKEN("4", Union, Declaration, 1);
    EXPECT_TOKEN("5", Union, Definition, 1);

    /// FIXME: Add more tests.
}

}  // namespace

}  // namespace clice::testing
