#include <gtest/gtest.h>
#include <AST/ParsedAST.h>

namespace {

using namespace clang;

TEST(test, test) {
    std::vector<const char*> compileArgs = {
        "clang++",
        "-std=c++20",
        "main.cpp",
        "-resource-dir=/home/ykiko/C++/clice2/build/lib/clang/20",
    };

    const char* code = R"(
int x [[maybe_unused]] = 0;
)";

    auto AST = clice::ParsedAST::build("main.cpp", code, compileArgs);
    auto fileID = AST->getFileID("main.cpp");
    auto tokens = AST->spelledTokens(fileID);

    // ClassVisitor visitor(&AST->context);
    // visitor.TraverseDecl(AST->context.getTranslationUnitDecl());

    for(auto& token: tokens) {
        llvm::outs() << clang::tok::getTokenName(token.kind()) << ": " << token.text(AST->sourceManager) << "\n";
    }
}

}  // namespace

