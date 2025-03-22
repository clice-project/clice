#include "Test/CTest.h"
#include "Index/FeatureIndex.h"

namespace clice::testing {

namespace {

TEST(FeatureIndex, SemanticTokens) {
    llvm::StringRef content = "int x = 1;";
    Tester tester("main.cpp", content);
    tester.run();

    auto index = index::indexFeature(*tester.info);

    // auto& m = index.at(tester.info->getInterestedFile());
    //
    // auto tokens = m.semanticTokens();
    //
    // EXPECT_EQ(tokens.size(), 3);
    // EXPECT_EQ(tokens[0].kind, SymbolKind::Keyword);
    // EXPECT_EQ(tokens[1].kind, SymbolKind::Variable);
    // EXPECT_EQ(tokens[2].kind, SymbolKind::Number);
}

}  // namespace

}  // namespace clice::testing
