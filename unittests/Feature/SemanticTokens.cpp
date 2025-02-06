#include "Test/CTest.h"
#include "Feature/SemanticTokens.h"

namespace clice::testing {

namespace {

struct SemanticTokens : ::testing::Test, Tester {
    index::Shared<std::vector<feature::SemanticToken>> result;

    void run(llvm::StringRef code) {
        addMain("main.cpp", code);
        Tester::run();
        result = feature::semanticTokens(*info);
    }

    void EXPECT_TOKEN(llvm::StringRef pos,
                      SymbolKind kind,
                      uint32_t length,
                      std::source_location current = std::source_location::current()) {
        bool visited = false;
        auto offset = offsets[pos];
        auto& tokens = result[info->getInterestedFile()];

        for(auto& token: tokens) {
            if(token.range.begin == offset) {
                EXPECT_EQ(token.kind, kind, current);
                EXPECT_EQ(token.range.end - token.range.begin, length, current);
                visited = true;
                break;
            }
        }

        EXPECT_EQ(visited, true, current);
    }
};

TEST_F(SemanticTokens, Include) {
    run(R"cpp(
$(0)#include $(1)<stddef.h>
$(2)#include $(3)"stddef.h"
$(4)# $(5)include $(6)"stddef.h"
)cpp");

    EXPECT_TOKEN("0", SymbolKind::Directive, 8);
    EXPECT_TOKEN("1", SymbolKind::Header, 10);
    EXPECT_TOKEN("2", SymbolKind::Directive, 8);
    EXPECT_TOKEN("3", SymbolKind::Header, 10);
    EXPECT_TOKEN("4", SymbolKind::Directive, 1);
    EXPECT_TOKEN("5", SymbolKind::Directive, 7);
    EXPECT_TOKEN("6", SymbolKind::Header, 10);
}

TEST_F(SemanticTokens, Comment) {
    run(R"cpp(
$(line)/// line comment
int x = 1;
)cpp");

    EXPECT_TOKEN("line", SymbolKind::Comment, 16);
}

TEST_F(SemanticTokens, Keyword) {
    run(R"cpp(
$(int)int main() {
    $(return)return 0;
}
)cpp");

    EXPECT_TOKEN("int", SymbolKind::Keyword, 3);
    EXPECT_TOKEN("return", SymbolKind::Keyword, 6);
}

TEST_F(SemanticTokens, Macro) {
    run(R"cpp(
$(0)#define $(macro)FOO
)cpp");

    EXPECT_TOKEN("0", SymbolKind::Directive, 7);
    EXPECT_TOKEN("macro", SymbolKind::Macro, 3);
}

TEST_F(SemanticTokens, FinalAndOverride) {
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

    EXPECT_TOKEN("0", SymbolKind::Keyword, 5);
    EXPECT_TOKEN("1", SymbolKind::Keyword, 8);
    EXPECT_TOKEN("2", SymbolKind::Keyword, 5);
}

}  // namespace

}  // namespace clice::testing

