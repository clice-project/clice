#include <gtest/gtest.h>
#include <Compiler/Resolver.h>
#include <clang/Sema/TreeTransform.h>

namespace {

using namespace clice;

std::vector<const char*> compileArgs = {
    "clang++",
    "-std=c++20",
    "main.cpp",
    "-resource-dir",
    "/home/ykiko/C++/clice2/build/lib/clang/20",
};

struct Visitor : public clang::RecursiveASTVisitor<Visitor> {
    clang::QualType result;
    std::unique_ptr<ParsedAST> parsedAST;

    Visitor(const char* code) : parsedAST(ParsedAST::build("main.cpp", code, compileArgs)) {}

    bool VisitTypeAliasDecl(clang::TypeAliasDecl* decl) {
        if(decl->getName() == "result") {
            TemplateResolver resolver(parsedAST->sema);
            result = resolver.resolve(decl->getUnderlyingType());
            result.dump();
        }
        return true;
    }

    clang::QualType test() {
        clang::TypeLocBuilder builder;
        auto decl = parsedAST->context.getTranslationUnitDecl();
        TraverseDecl(decl);
        return result;
    }
};

TEST(Transform, test) {

    const char* code = R"(
template <typename ...Ts>
struct type_list {};

template <typename X, typename Y, typename Z = void>
struct A {
    using type = type_list<X, Y, Z>;
};

template <typename X, typename Y = type_list<X>>
struct test {
    using base = A<X, Y>;
    using result = typename base::type;
};

)";

    Visitor visitor(code);
    auto result = visitor.test();
}

}  // namespace

