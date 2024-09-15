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

std::vector<const char*> compileArgs = {
    "clang++",
    "-std=c++20",
    "main.cpp",
    "-resource-dir=/home/ykiko/C++/clice2/build/lib/clang/20",
};

TEST(test, test) {

    const char* code = R"(
template<typename T>
struct X {
    using type = T;
    static constexpr bool value = true;
};

template <typename T>
auto f() {
    using type = typename X<T>::type::type;
    return T::value;
}
)";

    auto AST = clice::ParsedAST::build("main.cpp", code, compileArgs);
    auto fileID = AST->getFileID("main.cpp");
    AST->context.getTranslationUnitDecl()->dump();
    auto semanticTokens = clice::feature::semanticTokens(*AST, "main.cpp");
}

}  // namespace

