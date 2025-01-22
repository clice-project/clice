#include "Test/CTest.h"
#include "Feature/SemanticTokens.h"

namespace clice::testing {

namespace {

struct SemanticTokens : ::testing::Test {
    std::optional<Tester> tester;
    index::Shared<std::vector<feature::SemanticToken>> result;

    struct InputCase {
        std::uint32_t offset;
        SymbolKind kind;
        std::source_location current;
    };

    std::vector<InputCase> cases;

    void run(llvm::StringRef code) {
        tester.emplace("main.cpp", code);
        tester->run();
        auto& info = tester->info;
        result = feature::semanticTokens(*info);
    }

    void EXPECT_TOKEN(llvm::StringRef pos,
                      SymbolKind kind,
                      std::source_location current = std::source_location::current()) {}

    void test() {}
};

TEST_F(SemanticTokens, Include) {
    run("#include <cstdio>");

    auto& tokens = result[tester->info->getInterestedFile()];
    print("Tokens: {}\n", dump(tokens));
}

}  // namespace

}  // namespace clice::testing

