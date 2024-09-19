#include <gtest/gtest.h>
#include <AST/ParsedAST.h>
#include <Feature/SemanticTokens.h>

namespace {

using namespace clang;

template <typename T>
auto f() {
    using type = typename T::type;
    return T::value;
}

TEST(test, test) {
    std::vector<const char*> compileArgs = {
        "clang++",
        "-std=c++20",
        "main.cpp",
        "-resource-dir=/home/ykiko/C++/clice2/build/lib/clang/20",
    };

#include <cstdio>

    const char* code = R"(
// 123
#if x
#endif
// 333
)";

    auto AST = clice::ParsedAST::build("main.cpp", code, compileArgs);
    auto fileID = AST->getFileID("main.cpp");
    // AST->context.getTranslationUnitDecl()->dump();
    // auto semanticTokens = clice::feature::semanticTokens(*AST, "main.cpp");
}

}  // namespace

