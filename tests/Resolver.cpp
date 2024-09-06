#include <AST/Resolver.h>
#include <gtest/gtest.h>

using namespace clice;

namespace {

struct ASTVisitor : public clang::RecursiveASTVisitor<ASTVisitor> {
    clang::Sema& sema;
    clang::QualType& result;
    clang::ASTContext& context;

    bool VisitTypeAliasDecl(clang::TypeAliasDecl* decl) {
        if(decl->getName() == "result") {
            auto type = decl->getUnderlyingType();
            {
                clang::Sema::CodeSynthesisContext context;
                context.Kind = clang::Sema::CodeSynthesisContext::TemplateInstantiation;
                context.Entity = decl;
                context.TemplateArgs = nullptr;
                sema.pushCodeSynthesisContext(context);
            }
            auto resolver = DependentNameResolver(sema, context);
            result = resolver.resolve(type);
        }
        return true;
    }
};

TEST(DependentNameResolver, resolve) {
    std::vector<const char*> args{
        "clang++",
        "-std=c++20",
        "main.cpp",
        "-resource-dir=../build/lib/clang/20",
    };

    const char* code = R"(
template <typename ...Ts>
struct type_list {};

template <typename T>
struct A {
    using type = type_list<T>;
};

template <typename X>
struct test {
    using result = typename A<X>::type;
};
)";

    auto parsedAST = ParsedAST::build("main.cpp", code, args);
    auto decl = parsedAST->context.getTranslationUnitDecl();

    clang::QualType result;

    ASTVisitor visitor{{}, parsedAST->sema, result, parsedAST->context};
    visitor.TraverseDecl(decl);

    {
        auto TST = result->getAs<clang::TemplateSpecializationType>();
        ASSERT_TRUE(TST);
        ASSERT_EQ(TST->getTemplateName().getAsTemplateDecl()->getName(), "type_list");

        auto args = TST->template_arguments();
        ASSERT_EQ(args.size(), 1);
        auto T = llvm::dyn_cast<clang::TemplateTypeParmType>(args[0].getAsType());
        ASSERT_TRUE(T);
        ASSERT_EQ(T->getDecl()->getName(), "X");
    }
}

}  // namespace

