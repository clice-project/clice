#include "Test/Tester.h"
#include "AST/Selection.h"

namespace clice::testing {

void EXPECT_SELECT(llvm::StringRef code,
                   llvm::StringRef kind,
                   LocationChain chain = LocationChain()) {
    Tester tester;
    tester.add_main("main.cpp", code);
    ASSERT_TRUE(tester.compile_with_pch(), chain);

    auto tree = SelectionTree::createRight(*tester.unit,
                                           {
                                               tester.point(),
                                               tester.point(),
                                           });

    auto node = tree.commonAncestor();
    ASSERT_TRUE(node, chain);

    auto [fid, range] = tester.unit->decompose_range(node->ASTNode.getSourceRange());
    EXPECT_EQ(range, tester.range(), chain);
    EXPECT_EQ(node->kind(), kind);
}

TEST(Selection, Point) {
    EXPECT_SELECT(R"(
template <typename T>
int x = @[T::$U::]ccc();
)",
                  "NestedNameSpecifierLoc");
}

TEST(Selection, InjectedClassName) {}

TEST(Selection, Metrics) {}

TEST(Selection, Selected) {}

TEST(Selection, PathologicalPreprocessor) {}

TEST(SelectionTest, IncludedFile) {}

TEST(SelectionTest, MacroArgExpansion) {}

TEST(SelectionTest, Implicit) {}

TEST(SelectionTest, DeclContextIsLexical) {}

TEST(SelectionTest, DeclContextLambda) {}

TEST(SelectionTest, UsingConcepts) {}

}  // namespace clice::testing
